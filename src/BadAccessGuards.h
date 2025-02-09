#pragma once

// 1. Define `BAD_ACCESS_GUARDS_ENABLE=1` for your **in-house** builds (you probably want it off in production...)
// 2. Declare the shadow memory that will hold the state and pointer to stack with `BA_GUARD_DECL(varname)`
// 3. For all (relevant) read operations of the container / object, use the scope guard `BA_GUARD_READ(varname)`.
// 4. For all (relevant) write operations of the container / object, use the scope guard `BA_GUARD_WRITE(varname)`. 
//   Do this only if it always writes! For example, don't use it on `operator[]` even though it returns a reference, use `BA_GUARD_READ` instead.
// 5. Add `BA_GUARD_DESTROY(varname)` at the beginning of the destructor.
// 6. Enjoy!
//
// You may optionally configure it with `BadAccessGuardSetConfig`.

#if !defined(BAD_ACCESS_GUARDS_ENABLE)
# if defined(NDEBUG)
#  define BAD_ACCESS_GUARDS_ENABLE 0
# else
#  define BAD_ACCESS_GUARDS_ENABLE 1
# endif
#endif

// Disable the guards when running ThreadSanitizer, we are racy by design!
#if defined(__has_feature) // Clang
#  if __has_feature(thread_sanitizer) // Can't be checked in same #if than defined or it will break old GCC versions
#   undef BAD_ACCESS_GUARDS_ENABLE
#   define BAD_ACCESS_GUARDS_ENABLE 0
#  endif
#elif defined(__SANITIZE_THREAD__) // GCC
# undef BAD_ACCESS_GUARDS_ENABLE
# define BAD_ACCESS_GUARDS_ENABLE 0
#endif

#if BAD_ACCESS_GUARDS_ENABLE

#include <stdint.h>

// Why we use those macros:
// - BA_GUARD_NO_INLINE: This is to limit performance impact on the fast path.
// - BA_GUARD_GET_PTR_IN_STACK: No other portable way to do it. This must return a pointer to the current stack. Expected to be faster than getting the thread Id (and works with fibers).
// - BA_GUARD_FORCE_INLINE: We want to reduce the overhead in debug builds as much as possible.
// - BA_GUARD_ATOMIC_RELAXED_LOAD/STORE_UPTR: We really don't want to use std::atomic for debug build performance.
//  On top of this, this avoids including std headers for project that may restrict its usage.
#if defined(_MSC_VER) // MSVC
# include <intrin.h> // Necessary for _AddressOfReturnAddress
# define BA_GUARD_NO_INLINE __declspec(noinline)
# define BA_GUARD_FORCE_INLINE __forceinline // Need to use /d2Obforceinline for MSVC 17.7+ debug builds, otherwise it doesnt work! Not compatible with /Od...
# define BA_GUARD_GET_PTR_IN_STACK() _AddressOfReturnAddress()
# ifdef _WIN64 // 64 bits
#  define BA_GUARD_ATOMIC_RELAXED_LOAD_UPTR(var) static_cast<uintptr_t>(__iso_volatile_load64(reinterpret_cast<volatile int64_t*>(&var)))
#  define BA_GUARD_ATOMIC_RELAXED_STORE_UPTR(var, value) __iso_volatile_store64(reinterpret_cast<volatile int64_t*>(&var), value)
# else // Assume 32 bits
#  define BA_GUARD_ATOMIC_RELAXED_LOAD_UPTR(var) static_cast<uintptr_t>(__iso_volatile_load32(reinterpret_cast<volatile int32_t*>(&var)))
#  define BA_GUARD_ATOMIC_RELAXED_STORE_UPTR(var, value) __iso_volatile_store32(reinterpret_cast<volatile int32_t*>(&var), value)
# endif
# define BA_GUARD_DEBUGBREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__GNUG__) // GCC / clang
# define BA_GUARD_FORCE_INLINE __attribute__((always_inline))
# define BA_GUARD_NO_INLINE __attribute__ ((noinline))
# define BA_GUARD_GET_PTR_IN_STACK() __builtin_frame_address(0)
# define BA_GUARD_ATOMIC_RELAXED_LOAD_UPTR(var) __atomic_load_n(&var, __ATOMIC_RELAXED);
# define BA_GUARD_ATOMIC_RELAXED_STORE_UPTR(var, value) __atomic_store_n(&var, value, __ATOMIC_RELAXED);
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

// Does not seem to have much impact on performance in simple benchmarks, but might as well use it.
#if defined(__has_cpp_attribute) && __has_cpp_attribute(unlikely) >= 201803L
# define BA_GUARD_UNLIKELY [[unlikely]]
#else
# define BA_GUARD_UNLIKELY
#endif


enum BadAccessGuardState : uintptr_t
{
    BAGuard_ReadingOrIdle = 0,
    BAGuard_Writing = 1,
    BAGuard_DestructorCalled = 2,
    BAGuard_StatesCount
};

using StateAndStackAddr = uintptr_t;
struct BadAccessGuardShadow
{
#ifdef _WIN32
    // On Windows all stacks are aligned to page boundaries (both address and size), so we can store the state as a byte to avoid masking!
    static constexpr int BadAccessStateBits = 8;
    static_assert((1 << BadAccessStateBits) <= 4096, "The number of bits used for the state must be smaller than page alignment");
#else
    // Note: On 64bits platforms we could store the state in the upper byte instead, as in userspace the 12 upper bits are unused
    //       Left as an exercise for the reader. 
    static constexpr int BadAccessStateBits = 2;
    static_assert((1<< BadAccessStateBits) <= alignof(uint32_t), "Assume the stack base and size are at most aligned to your CPU native alignement");
#endif
    static_assert(BAGuard_StatesCount <= (1 << BadAccessStateBits), "BadAccessGuardState must fit in the lower bits");

    static constexpr StateAndStackAddr BadAccessStateMask = (1 << BadAccessStateBits) - 1;
    static constexpr StateAndStackAddr InStackAddrMask = StateAndStackAddr(-1) ^ BadAccessStateMask;

    StateAndStackAddr stateAndInStackAddr{ BAGuard_ReadingOrIdle };

    // Not in StateAndStackAddr, as we want to be setting 
    BA_GUARD_FORCE_INLINE void SetStateAtomicRelaxed(BadAccessGuardState newState)
    {
        // All in a single line for debug builds... sorry !
        BA_GUARD_ATOMIC_RELAXED_STORE_UPTR(stateAndInStackAddr, (StateAndStackAddr(BA_GUARD_GET_PTR_IN_STACK()) & InStackAddrMask) | StateAndStackAddr(newState));
    }
    // Those are static because we want to work on copies of the data and not pay for the atomic access
    static BA_GUARD_FORCE_INLINE BadAccessGuardState GetState(StateAndStackAddr packedValue) { return BadAccessGuardState(packedValue & BadAccessStateMask); }
    static BA_GUARD_FORCE_INLINE void* GetInStackAddr(StateAndStackAddr packedValue) { return (void*)StateAndStackAddr(packedValue & InStackAddrMask); }
};

// We have two versions to reduce code size at call site
void BA_GUARD_NO_INLINE BAGuardHandleBadAccess(StateAndStackAddr previousOperation, BadAccessGuardState toState, bool assertionOrWarning, const char* message);
// Both inline and no_inline! inline is necessary because we define it in a header, but still we don't actually want to inline it, hence no-inline.
inline void BA_GUARD_NO_INLINE BAGuardHandleBadAccess(StateAndStackAddr previousOperation, BadAccessGuardState toState) { BAGuardHandleBadAccess(previousOperation, toState, true, nullptr); }

struct BadAccessGuardRead
{
    // We have two versions of the constructor purely for performance
    BA_GUARD_FORCE_INLINE BadAccessGuardRead(BadAccessGuardShadow& shadow)
    {
        const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_UPTR(shadow.stateAndInStackAddr);
        if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_ReadingOrIdle) BA_GUARD_UNLIKELY // Early out on fast path
        {
            BAGuardHandleBadAccess(lastSeenOp, BAGuard_ReadingOrIdle);
        }
    }
    BA_GUARD_FORCE_INLINE BadAccessGuardRead(BadAccessGuardShadow& shadow, bool assertionOrWarning, char* message)
    {
        const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_UPTR(shadow.stateAndInStackAddr);
        if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_ReadingOrIdle) BA_GUARD_UNLIKELY// Early out on fast path
        {
            BAGuardHandleBadAccess(lastSeenOp, BAGuard_ReadingOrIdle, assertionOrWarning, message);
        }
    }
    // We do not check again after the read itself, it would add too much cost for little benefit. Most of the issues will be caught by the write ops.
};

struct BadAccessGuardWrite
{
    BadAccessGuardShadow& shadow;
    BA_GUARD_FORCE_INLINE BadAccessGuardWrite(BadAccessGuardShadow& shadow)
        : shadow(shadow)
    {
        const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_UPTR(shadow.stateAndInStackAddr);
        if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_ReadingOrIdle) BA_GUARD_UNLIKELY
        {
            BAGuardHandleBadAccess(lastSeenOp, BAGuard_Writing);
        }
        shadow.SetStateAtomicRelaxed(BAGuard_Writing); // Always write, so that we may trigger in other thread too
    }
    BA_GUARD_FORCE_INLINE ~BadAccessGuardWrite()
    {
        const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_UPTR(shadow.stateAndInStackAddr);
        if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_Writing) BA_GUARD_UNLIKELY
        {
            BAGuardHandleBadAccess(lastSeenOp, BAGuard_Writing);
        }
        shadow.SetStateAtomicRelaxed(BAGuard_ReadingOrIdle);
    }
};

// Same as BadAccessGuardWrite, but with additional options
struct BadAccessGuardWriteEx
{
    BadAccessGuardShadow& shadow;
    const char* const message;
    const bool assertionOrWarning;
    BA_GUARD_FORCE_INLINE BadAccessGuardWriteEx(BadAccessGuardShadow& d, bool assertionOrWarning = false, char* message = nullptr)
        : shadow(d)
        , message(message)
        , assertionOrWarning(assertionOrWarning)
    {
        const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_UPTR(shadow.stateAndInStackAddr);
        if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_ReadingOrIdle) BA_GUARD_UNLIKELY
        {
            BAGuardHandleBadAccess(lastSeenOp, BAGuard_Writing, assertionOrWarning, message);
        }
        shadow.SetStateAtomicRelaxed(BAGuard_Writing); // Always write, may trigger on other thread too
    }
    BA_GUARD_FORCE_INLINE ~BadAccessGuardWriteEx()
    {
        const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_UPTR(shadow.stateAndInStackAddr);
        if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_Writing) BA_GUARD_UNLIKELY
        {
            BAGuardHandleBadAccess(lastSeenOp, BAGuard_Writing, assertionOrWarning, message);
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
        const StateAndStackAddr lastSeenOp = BA_GUARD_ATOMIC_RELAXED_LOAD_UPTR(shadow.stateAndInStackAddr);
        if (BadAccessGuardShadow::GetState(lastSeenOp) != BAGuard_ReadingOrIdle) BA_GUARD_UNLIKELY
        {
            BAGuardHandleBadAccess(lastSeenOp, BAGuard_Writing);
        }
        shadow.SetStateAtomicRelaxed(BAGuard_DestructorCalled); // Always write
    }
};

struct BadAccessGuardConfig
{
    // Should we allow to break at all, or simply call `reportBadAccess`
    // Default: true.
    bool allowBreak;
    // Set this to true if you want to break early.
    // Usually you would want to set this to true when the debugger is connected.
    // If no debugger is connected, you most likely want this set to false to get the error in your logs.
    // Of course, if you save minidumps, logging is probably unnecessary.
    // Default: true on Windows if a debugger is detected during startup, false otherwise.
    bool breakASAP;

    // If non-null, used to report errors instead of the default function.
    // Breaking is still controlled by `allowBreak` and `breakASAP`.
    // Returning false can prevent triggering the breakpoint (except if `breakASAP` is true)
    using ReportBadAccessFunction = bool(StateAndStackAddr previousOperation, BadAccessGuardState toState, bool assertionOrWarning, const char* message);
    ReportBadAccessFunction* reportBadAccess;
};

// Check BAD_ACCESS_GUARDS_ENABLE if you want to use those
BadAccessGuardConfig BadAccessGuardGetConfig();
void BadAccessGuardSetConfig(BadAccessGuardConfig config);

#define BA_GUARD_DECL(SHADOWNAME)                               mutable BadAccessGuardShadow SHADOWNAME
#define BA_GUARD_READ(SHADOWNAME)                               BadAccessGuardRead BAGuardRead_##SHADOWNAME{SHADOWNAME}
#define BA_GUARD_READ_EX(SHADOWNAME,ASSERT_OR_WARN,MESSAGE)     BadAccessGuardRead BAGuardRead_##SHADOWNAME{SHADOWNAME, (ASSERT_OR_WARN), (MESSAGE)}
#define BA_GUARD_WRITE(SHADOWNAME)                              BadAccessGuardWrite BAGuardWrite_##SHADOWNAME{SHADOWNAME}
#define BA_GUARD_WRITE_EX(SHADOWNAME,ASSERT_OR_WARN,MESSAGE)    BadAccessGuardWriteEx BAGuardWriteEx_##SHADOWNAME{SHADOWNAME, (ASSERT_OR_WARN), (MESSAGE)}
#define BA_GUARD_DESTROY(SHADOWNAME)                            BadAccessGuardDestroy BAGuardDestroy_##SHADOWNAME{SHADOWNAME}

#else // BAD_ACCESS_GUARDS_ENABLE

#define BA_GUARD_DECL(SHADOWNAME)
#define BA_GUARD_READ(SHADOWNAME)                               do {} while(false)
#define BA_GUARD_READ_EX(SHADOWNAME,ASSERT_OR_WARN,MESSAGE)     do {} while(false)
#define BA_GUARD_WRITE(SHADOWNAME)                              do {} while(false)
#define BA_GUARD_WRITE_EX(SHADOWNAME,ASSERT_OR_WARN,MESSAGE)    do {} while(false)
#define BA_GUARD_DESTROY(SHADOWNAME)                            do {} while(false)

#endif // BAD_ACCESS_GUARDS_ENABLE