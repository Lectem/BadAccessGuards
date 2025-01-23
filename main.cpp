#include <stdint.h>

// Why we use those macros:
// - BA_GUARD_NO_INLINE: This is to limit performance impact on the fast path.
// - BA_GUARD_GET_PTR_IN_STACK: No other portable way to do it. This must return a pointer to the current stack. Expected to be faster than getting the thread Id (and works with fibers).
// - BA_GUARD_FORCE_INLINE: We want to reduce the overhead in debug builds as much as possible.
// - BA_GUARD_ATOMIC_RELAXED_LOAD/STORE_U64: We really don't want to use std::atomic for debug build performance.
//  On top of this, this avoids including std headers for project that may restrict its usage.
#if defined(_MSC_VER) // MSVC
# include <intrin.h> // Necessary for _AddressOfReturnAddress
# define BA_GUARD_NO_INLINE __declspec(noinline)
# define BA_GUARD_FORCE_INLINE __forceinline // Need to use /d2Obforceinline for MSVC 17.7+ debug builds, otherwise it doesnt work!
# define BA_GUARD_GET_PTR_IN_STACK() _AddressOfReturnAddress()
# define BA_GUARD_ATOMIC_RELAXED_LOAD_U64(var) static_cast<uint64_t>(__iso_volatile_load64(reinterpret_cast<volatile int64_t*>(&var)))
# define BA_GUARD_ATOMIC_RELAXED_STORE_U64(var, value) __iso_volatile_store64(reinterpret_cast<volatile int64_t*>(&var), value)
# define BA_GUARD_DEBUGBREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__GNUG__) // GCC / clang
# define BA_GUARD_FORCE_INLINE __attribute__((always_inline))
# define BA_GUARD_NO_INLINE __attribute__ ((noinline))
# define BA_GUARD_GET_PTR_IN_STACK() __builtin_frame_address(0) // Is this the right thing ?
# define BA_GUARD_ATOMIC_RELAXED_LOAD_U64(var) __atomic_load_n(&var, __ATOMIC_RELAXED);
# define BA_GUARD_ATOMIC_RELAXED_STORE_U64(var, value) __atomic_store_n(&var, value, __ATOMIC_RELAXED);
# if defined(__clang__)
#  define BA_GUARD_DEBUGBREAK() __builtin_debugtrap()
# else
// FFS, it's high time GCC implemented __builtin_debugtrap... maybe they will thanks to C++26 adding std::breakpoint?
// Instead we have to write our own assembly.
#  if defined(__i386__) || defined(__x86_64__)
#   define BA_GUARD_DEBUGBREAK() __asm__ volatile("int $0x03")
#  elif defined(__thumb__)
#   define BA_GUARD_DEBUGBREAK() __asm__ volatile(".inst 0xde01")
#  elif defined(__arm__) && !defined(__thumb__)
#   define BA_GUARD_DEBUGBREAK() __asm__ volatile(".inst 0xe7f001f0")
#  else
#   include <cassert>
#   define BA_GUARD_DEBUGBREAK() assert(false);
#  endif
# endif
#else // Are there really other compilers that don't implement one or the other nowadays? Maybe Intel/NVidia compilers... Well, up to you to implement it for those.
# error "Unknown compiler"
#endif

enum BadAccessGuardState : uint64_t
{
	BAGuard_ReadingOrIdle = 0,
	BAGuard_Writing = 1,
	BAGuard_DestructorCalled = 2,
	BAD_ACCESS_STATES_COUNT
};

using StateAndStackAddr = uintptr_t;
struct BadAccessGuardShadow
{
	static constexpr int BadAccessStateBits = 2;
	static constexpr StateAndStackAddr BadAccessStateMask = (1 << BadAccessStateBits) - 1;
	static constexpr StateAndStackAddr InStackAddrMask = StateAndStackAddr(-1) ^ BadAccessStateMask;
	static_assert(BAD_ACCESS_STATES_COUNT <= (1 << BadAccessStateBits), "BadAccessGuardState must fit in the lower bits");
	static_assert(BAD_ACCESS_STATES_COUNT <= alignof(uint32_t), "Assume the stack base and size are at most aligned to your CPU native alignement");

	StateAndStackAddr stateAndInStackAddr{ BAGuard_ReadingOrIdle };

	// Not in StateAndStackAddr, as we want to be setting 
	BA_GUARD_FORCE_INLINE void SetStateAtomicRelaxed(BadAccessGuardState newState)
	{
		// All in a single line for debug builds... sorry !
		BA_GUARD_ATOMIC_RELAXED_STORE_U64(stateAndInStackAddr, (uintptr_t(BA_GUARD_GET_PTR_IN_STACK()) & InStackAddrMask) | uint64_t(newState));
	}
	// Those are static because we want to work on copies of the data and not pay for the atomic access
	static BA_GUARD_FORCE_INLINE BadAccessGuardState GetState(StateAndStackAddr packedValue) { return BadAccessGuardState(packedValue & BadAccessStateMask); }
	static BA_GUARD_FORCE_INLINE void* GetInStackAddr(StateAndStackAddr packedValue) { return (void*)StateAndStackAddr(packedValue & InStackAddrMask); }
};

// We take both a ref to the guard and a copie of its previous value to avoid volatile accesses.
void BA_GUARD_NO_INLINE OnBadAccess(StateAndStackAddr previousOperation, BadAccessGuardState toState, bool assertionOrWarning = false, const char* message = nullptr);

struct BadAccessGuardRead
{
	BA_GUARD_FORCE_INLINE BadAccessGuardRead(BadAccessGuardShadow& shadow, bool assertionOrWarning = false, char* message = nullptr)
	{
		const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_U64(shadow.stateAndInStackAddr);
		if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_ReadingOrIdle) // Early out on fast path
		{
			OnBadAccess(lastSeenOp, BAGuard_ReadingOrIdle, assertionOrWarning, message);
		}
	}
	// We do not check again after the read itself, it would add too much cost for little benefits. Most of the issues will be caught by the write ops.
};

struct BadAccessGuardWrite
{
	BadAccessGuardShadow& shadow;
	BA_GUARD_FORCE_INLINE BadAccessGuardWrite(BadAccessGuardShadow& shadow)
		: shadow(shadow)
	{
		const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_U64(shadow.stateAndInStackAddr);
		if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_ReadingOrIdle)
		{
			OnBadAccess(lastSeenOp, BAGuard_Writing);
		}
		shadow.SetStateAtomicRelaxed(BAGuard_Writing); // Always write, so that we may trigger in other thread too
	}
	inline	~BadAccessGuardWrite()
	{
		const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_U64(shadow.stateAndInStackAddr);
		if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_Writing)
		{
			OnBadAccess(lastSeenOp, BAGuard_Writing);
		}
		shadow.SetStateAtomicRelaxed(BAGuard_ReadingOrIdle);
	}
};
struct BadAccessGuardDestroy
{
	BadAccessGuardShadow& shadow;
	BA_GUARD_FORCE_INLINE BadAccessGuardDestroy(BadAccessGuardShadow& shadow)
		: shadow(shadow)
	{
		const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_U64(shadow.stateAndInStackAddr);
		if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_ReadingOrIdle)
		{
			OnBadAccess(lastSeenOp, BAGuard_Writing);
		}
		shadow.SetStateAtomicRelaxed(BAGuard_DestructorCalled); // Always write
	}
};

struct BadAccessGuardIgnoreDestroy
{
	BadAccessGuardShadow& shadow;
	BA_GUARD_FORCE_INLINE BadAccessGuardIgnoreDestroy(BadAccessGuardShadow& shadow)
		: shadow(shadow)
	{
		shadow.SetStateAtomicRelaxed(BAGuard_ReadingOrIdle);
	}
};

struct BadAccessGuardWriteEx
{
	BadAccessGuardShadow& shadow;
	const char* const messsage;
	const bool assertionOrWarning;
	BA_GUARD_FORCE_INLINE BadAccessGuardWriteEx(BadAccessGuardShadow& d, bool assertionOrWarning = false, char* messsage = nullptr)
		: shadow(d)
		, messsage(messsage)
		, assertionOrWarning(assertionOrWarning)
	{
		const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_U64(shadow.stateAndInStackAddr);
		if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_ReadingOrIdle)
		{
			OnBadAccess(lastSeenOp, BAGuard_Writing, assertionOrWarning, messsage);
		}
		shadow.SetStateAtomicRelaxed(BAGuard_ReadingOrIdle); // Always write, may trigger on other thread too
	}
	BA_GUARD_FORCE_INLINE ~BadAccessGuardWriteEx()
	{
		const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_U64(shadow.stateAndInStackAddr);
		if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_Writing)
		{
			OnBadAccess(lastSeenOp, BAGuard_Writing, assertionOrWarning, messsage);
		}
		shadow.SetStateAtomicRelaxed(BAGuard_ReadingOrIdle);
	}
};


///////////////////////////////
////// DEFAULT IMPL FILE //////
///////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

// Per OS thread information retrieval
using ThreadDescBuffer = char[512];

#ifdef _WIN32 // Windows 
#include <Windows.h>

bool IsAddressInStack(NT_TIB* tib, void* ptr)
{
	return tib->StackLimit <= ptr && ptr <= tib->StackBase;
}
bool IsAddressInCurrentStack(void* ptr)
{
	NT_TIB* tib = (NT_TIB*)NtCurrentTeb(); // NT_TEB starts with NT_TIB for all usermode threads. See ntddk.h.
	return IsAddressInStack(tib, ptr);
}

#include <tlhelp32.h>
#include <winternl.h>
#pragma comment(lib, "ntdll.lib")
uint64_t FindThreadWithPtrInStack(void* ptr, ThreadDescBuffer outDescription)
{
	outDescription[0] = '\0';

	HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
	THREADENTRY32 te32;

	// Take a snapshot of all running threads  
	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
		return NULL;


	// Fill in the size of the structure before using it. 
	te32.dwSize = sizeof(THREADENTRY32);

	DWORD idOfThreadWithAddrInStack = 0; // 0 is an invalid thread id
	const DWORD dwOwnerPID = GetCurrentProcessId();
	for(BOOL bHasThread = Thread32First(hThreadSnap, &te32); 
		bHasThread && !idOfThreadWithAddrInStack; 
		bHasThread = Thread32Next(hThreadSnap, &te32))
	{
		if (te32.th32OwnerProcessID == dwOwnerPID)
		{
			const THREADINFOCLASS ThreadBasicInformation = THREADINFOCLASS(0);

			typedef LONG KPRIORITY;

			typedef struct _CLIENT_ID {
				HANDLE UniqueProcess;
				HANDLE UniqueThread;
			} CLIENT_ID;
			typedef CLIENT_ID* PCLIENT_ID;

			typedef struct _THREAD_BASIC_INFORMATION
			{
				NTSTATUS                ExitStatus;
				PVOID                   TebBaseAddress;
				CLIENT_ID               ClientId;
				KAFFINITY               AffinityMask;
				KPRIORITY               Priority;
				KPRIORITY               BasePriority;
			} THREAD_BASIC_INFORMATION, * PTHREAD_BASIC_INFORMATION;


			if (HANDLE threadHdl = OpenThread(THREAD_QUERY_INFORMATION|THREAD_QUERY_LIMITED_INFORMATION, FALSE, te32.th32ThreadID))
			{
				// Get TEB address
				THREAD_BASIC_INFORMATION basicInfo;
				NtQueryInformationThread(threadHdl, ThreadBasicInformation, &basicInfo, sizeof(THREAD_BASIC_INFORMATION), NULL);
				if (IsAddressInStack((NT_TIB*)basicInfo.TebBaseAddress, ptr))
				{
					idOfThreadWithAddrInStack = te32.th32ThreadID;
					PWSTR desc;
					if (SUCCEEDED(GetThreadDescription(threadHdl, &desc)))
					{
						int nbChars = WideCharToMultiByte(GetConsoleCP(), 0, desc, -1, outDescription, sizeof(ThreadDescBuffer) - 1, NULL, NULL);
						outDescription[nbChars] = '\0'; // Make sure to have a null terminated string. nbChars is 0 on error => Empty string
						LocalFree(desc);
					}
				}
				CloseHandle(threadHdl);

			}
		}
	}
	CloseHandle(hThreadSnap);     // Must clean up the snapshot object!
	return idOfThreadWithAddrInStack;
}

#elif defined(_GNU_SOURCE) && (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) // Linux / POSIX
#include <pthread.h>
bool IsAddressInCurrentStack(void* ptr)
{
	// TODO: handle failure properly. For now return false on failure. (Assume this is a MT error)
	pthread_attr_t attributes;
	if (0 != pthread_getattr_np(pthread_self(), &attributes)) return false;

	void* stackAddr;
	size_t stackSize;
	if (0 != pthread_attr_getstack(&attributes, &stackAddr, &stackSize)) return false;

	// On linux, address is indeed the start address (what you would give if allocating yourself)
	return stackAddr <= ptr && uintptr_t(ptr) < (uintptr_t(stackAddr) + stackSize);
}
uint64_t FindThreadWithPtrInStack(void* ptr, ThreadDescBuffer outDescription) { outDescription[0] = '\0'; return 0; }
#elif defined(__APPLE__) // Apple, why do you make it so hard to look for your posix_*_np functions... Just give us docs or something instead of having us dive into the darwin-libpthread code! Didn't bother goind further and try to compile/run this.
#include <pthread.h>
bool IsAddressInCurrentStack(void* ptr)
{
	pthread_t currentThread = pthread_self();
	void* stackAddr = pthread_get_stackaddr_np(currentThread);
	size_t stackSize = pthread_get_stacksize_np(currentThread);
	// Stack grows downards, see https://github.com/apple/darwin-libpthread/blob/2b46cbcc56ba33791296cd9714b2c90dae185ec7/src/pthread.c#L979
	// The two functions above do NOT match the pthread_attr_tt attributes! https://github.com/apple/darwin-libpthread/blob/2b46cbcc56ba33791296cd9714b2c90dae185ec7/src/pthread.c#L476
	return (uintptr_t(stackAddr) - stackSize) <= uintptr_t(ptr) && ptr <= stackAddr;
}
uint64_t FindThreadWithPtrInStack(void* ptr, ThreadDescBuffer outDescription) { outDescription[0] = '\0'; return 0; }
#else
bool IsAddressInCurrentStack(void* ptr) { return false; } // Who knows ?
uint64_t FindThreadWithPtrInStack(void* ptr, ThreadDescBuffer outDescription) { outDescription[0] = '\0'; return 0; }
#endif

struct BadAccessGuardConfig
{

	BadAccessGuardConfig() :
#ifdef WIN32
		breakASAP(IsDebuggerPresent())
#else
		breakASAP(false)
#endif
	{}

	// Set this to true if you want to break early.
	// Usually you would want to set this to true when the debugger is connected.
	// If no debugger is connected, you most likely want this set to false to get the error in your logs.
	// Of course, if you save minidumps, logging is probably unnecessary.
	bool breakASAP;
} gBadAccessGuardConfig;



// Return true if you want to break (unless breakASAP is set)
bool BadAccessGuardReport(bool assertionOrWarning, const char* fmt, ...);

void BA_GUARD_NO_INLINE OnBadAccess(StateAndStackAddr previousOperation, BadAccessGuardState toState, bool assertionOrWarning, const char* message)
{
	// If you break here it means that we detected some bad memory access pattern
	// It could be that you are mutating a container recursively or a multi-threading race condition
	// You can now:
	// - Step/Continue to get information about the error
	// - Inspect other threads callstacks (If using Visual Studio: Debug => Windows => Parallel Stacks)
	//   If the debugger broke and froze the other threads fast enough, you might be able to find the offending thread.
	if (assertionOrWarning && gBadAccessGuardConfig.breakASAP) BA_GUARD_DEBUGBREAK(); // Break asap in an attempt to catch the other thread in the act !
	const bool previousState = BadAccessGuardShadow::GetState(previousOperation);
	const bool fromSameThread = IsAddressInCurrentStack(BadAccessGuardShadow::GetInStackAddr(previousOperation));
	bool shouldBreak = assertionOrWarning;
	if (message)
	{
		shouldBreak = BadAccessGuardReport(assertionOrWarning, message);
	}
	else
	{
		const char* stateToStr[] = {
			toState == BAGuard_Writing ? "Writing" : "Reading", // The only cases when we can see this state are when we try to read, or in the writing guard destructor, if another write ends before. So we know that it can only be due to a write.
			"Writing",
			"Destroyed"
		};
		static_assert(sizeof(stateToStr) / sizeof(stateToStr[0]) == BAD_ACCESS_STATES_COUNT, "Mismatch, new state added ?");

		if (fromSameThread)
		{

			shouldBreak = BadAccessGuardReport(assertionOrWarning, "Recursion detected: This may lead to invalid operations\n- Parent operation: %s.\n- This operation: %s.", stateToStr[previousState], stateToStr[toState]);
		}
		else
		{
			ThreadDescBuffer outDescription;
			uint64_t otherThreadId = FindThreadWithPtrInStack(BadAccessGuardShadow::GetInStackAddr(previousOperation), outDescription);
			shouldBreak = BadAccessGuardReport(assertionOrWarning,
				"Race condition: Multiple threads are reading/writing to the data at the same time, potentially corrupting it!\n- Other thread: %s (Desc=%s Id=%u)\n- This thread %s.",
				stateToStr[previousState],
				outDescription[0] != '\0' ? outDescription : "<Unknown>",
				otherThreadId,
				stateToStr[toState]
			);

		}

	}

	if (assertionOrWarning && shouldBreak && !gBadAccessGuardConfig.breakASAP)
	{
		BA_GUARD_DEBUGBREAK();
	}
}

#include <cassert>
bool BadAccessGuardReport(bool assertionOrWarning, const char* fmt, ...)
{
	if (assertionOrWarning) assert(false);
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	// Let the caller break
	return true;
}



BadAccessGuardShadow someglobal;



#include <thread>

int main()
{
	{
		printf("Testing read during write on the same thread, output:\n");

		BadAccessGuardWrite writeg{ someglobal };
		BadAccessGuardRead readg{ someglobal };
	}


	{
		printf("\n\nTesting read during write on different threads, output:\n");

		using namespace std::chrono_literals;
		std::thread otherthread([] {
#ifdef _WIN32
			SetThreadDescription(GetCurrentThread(), L"ØUnsafe WriterØ");
#endif
			BadAccessGuardWrite writeg{ someglobal };
			std::this_thread::sleep_for(3s);
			});
		std::this_thread::sleep_for(1s);
		// Assuming the other thread has been scheduled. No sync on purpose since we want to see if our code could detect the race conditon.
		BadAccessGuardRead readg{ someglobal };
		otherthread.join();
	}


	{
		printf("\n\nTesting write during write on different threads, output:\n");

		using namespace std::chrono_literals;
		std::thread otherthread([] {
#ifdef _WIN32
			SetThreadDescription(GetCurrentThread(), L"ØUnsafe WriterØ");
#endif
			BadAccessGuardWrite writeg{ someglobal };
			std::this_thread::sleep_for(3s);
			});
		std::this_thread::sleep_for(1s);
		// Assuming the other thread has been scheduled. No sync on purpose since we want to see if our code could detect the race conditon.
		BadAccessGuardWrite readg{ someglobal };
		otherthread.join();
	}


	return 0;
}
