#include <stdint.h>

// Why we use those macros:
// - BA_GUARD_NO_INLINE: This is to limit performance impact on the fast path.
// - BA_GUARD_GET_PTR_IN_STACK: No other portable way to do it. This must return a pointer to the current stack. Expected to be faster than getting the thread Id (and works with fibers).
// - BA_GUARD_FORCE_INLINE: We want to reduce the overhead in debug builds as much as possible.
// - BA_GUARD_ATOMIC_RELAXED_LOAD/STORE_U64: We really don't want to use std::atomic for debug build performance.
//   On top of this, this avoids including std headers for project that may restrict its usage.
#if defined(_MSC_VER) // MSVC
#include <intrin.h> // Necessary for _AddressOfReturnAddress
#define BA_GUARD_NO_INLINE __declspec(noinline)
#define BA_GUARD_FORCE_INLINE __forceinline // Need to use /d2Obforceinline for MSVC 17.7+ debug builds, otherwise it doesnt work!
#define BA_GUARD_GET_PTR_IN_STACK() _AddressOfReturnAddress()
#define BA_GUARD_ATOMIC_RELAXED_LOAD_U64(var) static_cast<uint64_t>(__iso_volatile_load64(reinterpret_cast<volatile int64_t*>(&var)))
#define BA_GUARD_ATOMIC_RELAXED_STORE_U64(var, value) __iso_volatile_store64(reinterpret_cast<volatile int64_t*>(&var), value)
#elif defined(__GNUC__) || defined(__GNUG__) // GCC / clang
#define BA_GUARD_FORCE_INLINE __attribute__((always_inline))
#define BA_GUARD_NO_INLINE __attribute__ ((noinline))
#define BA_GUARD_GET_PTR_IN_STACK() __builtin_frame_address(0) // Is this the right thing ?
#define BA_GUARD_ATOMIC_RELAXED_LOAD_U64(var) __atomic_load_n(&var, __ATOMIC_RELAXED);
#define BA_GUARD_ATOMIC_RELAXED_STORE_U64(var, value) __atomic_store_n(&var, value, __ATOMIC_RELAXED);
#else // Are there really other compilers that don't implement one or the other nowadays? Maybe Intel/NVidia compilers... Well, up to you to implement it for those.
#error "Unknown compiler"
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

// Per OS thread retrieval from stack pointer
#ifdef _WIN32 // Windows 

#include <Windows.h>

bool IsAddressInCurrentStack(void* ptr)
{
	NT_TIB* tib = (NT_TIB*)NtCurrentTeb(); // NT_TEB starts with NT_TIB for all usermode threads. See ntddk.h.
	return tib->StackLimit <= ptr && ptr <= tib->StackBase;
}

#elif defined(_GNU_SOURCE) && (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) // Linux / POSIX
#include <pthread.h>
bool IsAddressInCurrentStack(void* ptr)
{
	// TODO: handle failure properly. For now return false on failure. (Assume this is a MT error)
	pthread_attr_t attributes;
	if(0!=pthread_getattr_np(pthread_self(), &attributes))
		return false;

	void* stackAddr;
	size_t stackSize;
	if (0 != pthread_attr_getstack(&attributes, &stackAddr, &stackSize))
		return false;
	// On linux, address is indeed the start address (what you would give if allocating yourself)
	return stackAddr <= ptr && uintptr_t(ptr) < (uintptr_t(stackAddr) + stackSize);
}
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
#else
bool IsAddressInCurrentStack(void* ptr) { return false; } // Who knows ?
#endif


#include <cassert>
void BadAccessGuardReport(bool assertionOrWarning, const char* fmt, ...)
{
	if (assertionOrWarning) assert(false);
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
}

void BA_GUARD_NO_INLINE OnBadAccess(StateAndStackAddr previousOperation, BadAccessGuardState toState, bool assertionOrWarning, const char* message)
{
	const bool previousState = BadAccessGuardShadow::GetState(previousOperation);
	const bool fromSameThread = IsAddressInCurrentStack(BadAccessGuardShadow::GetInStackAddr(previousOperation));

	if (message)
	{
		BadAccessGuardReport(assertionOrWarning, message);
	}
	else
	{
		const char* stateToStr[] = { 
			toState == BAGuard_Writing ? "Writing" : "Reading", // The only cases when we can see this state are when we try to read, or in the writing guard destructor, if another write ends before. So we know that it can only be due to a write.
			"Writing", 
			"Destroyed"
		};
		static_assert(sizeof(stateToStr) / sizeof(stateToStr[0]) == BAD_ACCESS_STATES_COUNT, "Mismatch, new state added ?");
		message = fromSameThread
			? "Recursion detected:\n- Parent operation: %s.\n- This operation: %s."
			: "Race condition: Multiple threads are reading/writing to the data at the same time, potentially corrupting it!\n- Other thread: %s\n- This thread %s.";
		BadAccessGuardReport(assertionOrWarning, message, stateToStr[previousState], stateToStr[toState]);
	}
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
