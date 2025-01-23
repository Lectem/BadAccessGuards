
///////////////////////////////
////// DEFAULT IMPL FILE //////
///////////////////////////////

#include "BadAccessGuards.h"
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
bool DefaultReportBadAccess(StateAndStackAddr previousOperation, BadAccessGuardState toState, bool assertionOrWarning, const char* message)
{
	const BadAccessGuardState previousState = BadAccessGuardShadow::GetState(previousOperation);
	const bool fromSameThread = IsAddressInCurrentStack(BadAccessGuardShadow::GetInStackAddr(previousOperation));
	if (message)
	{
		return BadAccessGuardReport(assertionOrWarning, message);
	}
	else if (previousState >= BAGuard_StatesCount)
	{
		return BadAccessGuardReport(assertionOrWarning, "Shadow value was corrupted! This could be due to use after-free, out of bounds writes, etc...");
	}
	else
	{
		const char* stateToStr[] = {
			toState == BAGuard_Writing ? "Writing" : "Reading", // The only cases when we can see this state are when we try to read, or in the writing guard destructor, if another write ends before. So we know that it can only be due to a write.
			"Writing",
			"Destroyed"
		};
		static_assert(sizeof(stateToStr) / sizeof(stateToStr[0]) == BAGuard_StatesCount, "Mismatch, new state added ?");

		if (fromSameThread)
		{

			return BadAccessGuardReport(assertionOrWarning, "Recursion detected: This may lead to invalid operations\n- Parent operation: %s.\n- This operation: %s.", stateToStr[previousState], stateToStr[toState]);
		}
		else
		{
			ThreadDescBuffer outDescription;
			uint64_t otherThreadId = FindThreadWithPtrInStack(BadAccessGuardShadow::GetInStackAddr(previousOperation), outDescription);
			return BadAccessGuardReport(assertionOrWarning,
				"Race condition: Multiple threads are reading/writing to the data at the same time, potentially corrupting it!\n- Other thread: %s (Desc=%s Id=%u)\n- This thread %s.",
				stateToStr[previousState],
				outDescription[0] != '\0' ? outDescription : "<Unknown>",
				otherThreadId,
				stateToStr[toState]
			);

		}

	}
}

void BA_GUARD_NO_INLINE OnBadAccess(StateAndStackAddr previousOperation, BadAccessGuardState toState, bool assertionOrWarning, const char* message)
{
	// If you break here it means that we detected some bad memory access pattern
	// It could be that you are mutating a container recursively or a multi-threading race condition
	// You can now:
	// - Step/Continue to get information about the error
	// - Inspect other threads callstacks (If using Visual Studio: Debug => Windows => Parallel Stacks)
	//   If the debugger broke and froze the other threads fast enough, you might be able to find the offending thread.
	if (assertionOrWarning && gBadAccessGuardConfig.breakASAP) BA_GUARD_DEBUGBREAK(); // Break asap in an attempt to catch the other thread in the act !
	
	const bool breakAllowed = DefaultReportBadAccess(previousOperation, toState, assertionOrWarning, message);

	if (assertionOrWarning && breakAllowed && !gBadAccessGuardConfig.breakASAP)	BA_GUARD_DEBUGBREAK();
}

bool BadAccessGuardReport(bool assertionOrWarning, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	// Let the caller break
	return true;
}

