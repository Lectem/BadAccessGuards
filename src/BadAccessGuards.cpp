// BadAccessGuards v1.0.0 https://github.com/Lectem/BadAccessGuards

//////////////////////////////////////////////////////////////////
//////                  DEFAULT IMPL FILE                   //////
////// Feel free to modify it as needed for your platforms! //////
////// Or if is is enough, just use BadAccessGuardSetConfig //////
//////////////////////////////////////////////////////////////////

#include "BadAccessGuards.h"

#if BAD_ACCESS_GUARDS_ENABLE

// Per OS thread information retrieval
using ThreadDescBuffer = char[512];

#ifdef _WIN32 // Windows 

#include <Windows.h>

bool IsAddressInStack(NT_TIB* tib, void* ptr)
{
    return tib->StackLimit <= ptr && ptr <= tib->StackBase;
}

bool IsAddressInStackSafe(NT_TIB* tib, void* ptr)
{
    __try
    {
        // If you break here with the debugger, it means that you're unlucky and the thread just closed right after we saw it
        // This can be ignored, but means that if this was the offending thread we will not be able to know it.
        return tib && tib->StackLimit <= ptr && ptr <= tib->StackBase;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool IsAddressInCurrentStack(void* ptr)
{
    NT_TIB* tib = (NT_TIB*)NtCurrentTeb(); // NT_TEB starts with NT_TIB for all usermode threads. See ntddk.h.
    return IsAddressInStack(tib, ptr);
}

#include <ProcessSnapshot.h>
// GetThreadDescription may not be in old versions of the SDK, 
// and if we want to be able to run on older versions of Windows, we need to dynamically load it anyway.
typedef HRESULT(WINAPI* GetThreadDescriptionPtrType)(HANDLE hThread, PWSTR* threadDescription);

uint64_t FindThreadWithPtrInStack(void* ptr, ThreadDescBuffer outDescription)
{
    // Attempt to get GetThreadDescription on first use of this function
    static GetThreadDescriptionPtrType GetThreadDescriptionPtr = (GetThreadDescriptionPtrType)GetProcAddress(GetModuleHandleA("KernelBase.dll"), "GetThreadDescription");

    // Clean description
    outDescription[0] = '\0';
    DWORD idOfThreadWithAddrInStack = 0; // 0 is an invalid thread id

    // Pss* functions are available for applications/drivers since Windows 8.1.
    // CreateToolhelp32Snapshot is only available on desktop but was available since Windows XP.
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_SYSTEM) && (NTDDI_VERSION >= NTDDI_WINBLUE)
    // Take a snapshot of all running threads.
    // Weirdly it does not necessarily see all threads (even if they are still alive and shown in the debugger)
    // It seems that threads need to be alive for a certain amount of time before it can actually see them.
    // CreateToolhelp32Snapshot suffers from the same issue.
    HPSS snapshotHandle;
    if (ERROR_SUCCESS != PssCaptureSnapshot(GetCurrentProcess(), PSS_CAPTURE_THREADS, 0, &snapshotHandle))
        return 0;
    
    HPSSWALK walkMarker;
    if (ERROR_SUCCESS == PssWalkMarkerCreate(nullptr, &walkMarker))
    {
        const DWORD dwOwnerPID = GetCurrentProcessId();
        PSS_THREAD_ENTRY threadEntry;
        while (idOfThreadWithAddrInStack == 0 // Stop if we find the thread
            && ERROR_SUCCESS == PssWalkSnapshot(snapshotHandle, PSS_WALK_THREADS, walkMarker, &threadEntry, sizeof(threadEntry)))
        {
            if (threadEntry.ProcessId != dwOwnerPID                         // This should never happen, but be safe
                || (threadEntry.Flags & PSS_THREAD_FLAGS_TERMINATED) != 0   // Thread was terminated, we can't access its PEB
                || threadEntry.ExitStatus != STILL_ACTIVE                   // Thread has exited, we can't access its PEB
                )
            {
                continue; // Just continue with the next threads
            }

            // The thread might die while we check its TEB, so we have a safe version that won't crash
            if (IsAddressInStackSafe((NT_TIB*)threadEntry.TebBaseAddress, ptr))
            {
                idOfThreadWithAddrInStack = threadEntry.ThreadId;
                if (HANDLE threadHdl = OpenThread(THREAD_QUERY_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION, FALSE, threadEntry.ThreadId))
                {
                    // PSS_WALK_THREAD_NAME sounds nice until you realize its not implemented.
                    // It's actually not recognized and you'll get a ERROR_INVALID_PARAMETER (at least on my version on Windows).
                    // That's probably why it's undocumented. Instead, we'll just use GetThreadDescription
                    PWSTR desc;
                    if (GetThreadDescriptionPtr && SUCCEEDED(GetThreadDescriptionPtr(threadHdl, &desc)))
                    {
                        const int nbChars = WideCharToMultiByte(GetConsoleCP(), 0, desc, -1, outDescription, sizeof(ThreadDescBuffer) - 1, NULL, NULL);
                        outDescription[nbChars] = '\0'; // Make sure to have a null terminated string. nbChars is 0 on error => Empty string
                        LocalFree(desc);
                    }
                    CloseHandle(threadHdl);
                }
            }
        }
        PssWalkMarkerFree(walkMarker);
    }

    PssFreeSnapshot(GetCurrentProcess(), snapshotHandle);
#endif
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

    // On POSIX, address is indeed the start address (what you would give if allocating yourself)
    return stackAddr <= ptr && uintptr_t(ptr) < (uintptr_t(stackAddr) + stackSize);
}

// There is no way to iterate threads and get their stack address + size.
// Attempted to get information for linux but failed, same conclusion as https://unix.stackexchange.com/questions/758975/how-can-i-locate-the-stacks-of-child-tasks-threads-using-proc-pid-maps
// So in the end we'd need to keep the threadid instead of just a pointer to the current thread stack, which would be too expensive.
// Just let the user inspect the stacks in the debugger instead.
// The alternative is for the user to keep track of their threads and implement this function themselves.
// We could also setup hooks to intercept pthread functions (this is what TSan does) but this is getting too heavy for this mini library.
uint64_t FindThreadWithPtrInStack(void* ptr, ThreadDescBuffer outDescription) { outDescription[0] = '\0'; return 0; }

#elif defined(__APPLE__) // MacOS / iOS
// Apple, why do you make it so hard to look for your posix_*_np functions... Just give us docs or something instead of having us dive into the darwin-libpthread code! Didn't bother going further and try to compile/run this.

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
// We could use https://developer.apple.com/documentation/kernel/1537751-task_threads + pthread_from_mach_thread_np + pthread_get_stackaddr/size_np
// Pull Requests are welcome!
uint64_t FindThreadWithPtrInStack(void* ptr, ThreadDescBuffer outDescription) { outDescription[0] = '\0'; return 0; }

#else // Unknown platform, default to assuming race conditions.

bool IsAddressInCurrentStack(void* ptr) { return false; } // Who knows ?
uint64_t FindThreadWithPtrInStack(void* ptr, ThreadDescBuffer outDescription) { outDescription[0] = '\0'; return 0; }

#endif

bool DefaultReportBadAccess(StateAndStackAddr previousOperation, BadAccessGuardState toState, bool assertionOrWarning, const char* message);

BadAccessGuardConfig gBadAccessGuardConfig{
    true, // allowBreak
#ifdef WIN32
    (bool)IsDebuggerPresent(), // breakASAP
#else
    // Note: Not implementing Linux as it seems there is no trival way to differentiate a tracer process that is a debugger from a profiler.
    //       Not implementing MacOs as would need to test
    false, // breakASAP
#endif
    DefaultReportBadAccess, // reportBadAccess
};

BadAccessGuardConfig BadAccessGuardGetConfig() { return gBadAccessGuardConfig; }
void BadAccessGuardSetConfig(BadAccessGuardConfig config)
{
    if (!config.reportBadAccess) {
        config.reportBadAccess = DefaultReportBadAccess;
    }
    gBadAccessGuardConfig = config;
}


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
            toState == BAGuard_Writing ? "Writing" : "Reading", // The only cases when we can see this state are when we try to read, or in the writing guard destructor which means another write ended before. So we know that it can only be due to a write (or corruption).
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
                "Race condition: Multiple threads are reading/writing to the data at the same time, potentially corrupting it!\n- Other thread: %s (Desc=%s Id=%llu)\n- This thread: %s.",
                stateToStr[previousState],
                outDescription[0] != '\0' ? outDescription : "<Unknown>",
                otherThreadId,
                stateToStr[toState]
            );

        }

    }
}

void BA_GUARD_NO_INLINE BAGuardHandleBadAccess(StateAndStackAddr previousOperation, BadAccessGuardState toState, bool assertionOrWarning, const char* message)
{
    // If you break here it means that we detected some bad memory access pattern
    // It could be that you are mutating a container recursively or a multi-threading race condition
    // You can now:
    // - Step/Continue to get information about the error (potentially waking offending threads if caused by a race condition)
    // - Inspect other threads callstacks (If using Visual Studio: Debug => Windows => Parallel Stacks)
    //   If the debugger broke and froze the other threads fast enough, you might be able to find the offending thread.
    if (assertionOrWarning && gBadAccessGuardConfig.allowBreak && gBadAccessGuardConfig.breakASAP) BA_GUARD_DEBUGBREAK(); // Break asap in an attempt to catch the other thread in the act !

    const bool breakAllowed = gBadAccessGuardConfig.reportBadAccess(previousOperation, toState, assertionOrWarning, message);

    if (assertionOrWarning && breakAllowed && gBadAccessGuardConfig.allowBreak && !gBadAccessGuardConfig.breakASAP)    BA_GUARD_DEBUGBREAK();
}

#include <stdio.h>
#include <stdarg.h>
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

#endif
