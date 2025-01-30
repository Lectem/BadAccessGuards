# BadAccessGuards

Finding race conditions and bad access patterns usually involves sanitizers or special programs such as valgrind.
Those are however seldom ran because they can have a heavy runtime performance cost.

This library makes it possible to detect most race conditions involving (but not only) containers through their instrumentation, at a minimal runtime cost.

As a bonus, we also get detection of memory use-after-free and corruption for free. This also detects recursive operations which are often dangerous in containers.

# Goals/Features

- Easy to integrate and modify for your project
  - There are only two files: the BadAccessGuards.h and BadAccessGuards.cpp
  - Licensed under the [Unlicence](LICENSE), you can just copy it without worrying about legal.
  - It does not include the C++ standard library, and can thus be used in your std-free libraries (or even for a standard library implementation!)
  - Small, there are only a few platform specific functions to implement
  - Supports MSVC, GCC and clang.
- Detect race conditions with minimal performance impact
  - You want to be able to run this in your day to day development builds
  - Adds only a few load/store/masks depending on the operations
  - Fast path (read) on windows is 2`mov`s+1`test`+`je`
  - Debug builds were given love
  - See [Benchmarks](#Benchmarks)
- No false positives that you wouldn't want to fix
- Provide details as accurate as possible
  - We detect if the access was done from another thread, and for platforms that allow it (Windows), print its information. We also give what kind of operation it was executing.
  - Break as early as possible to hopefully be able to inspect the other threads in the debugger.
- No dependencies other than your compiler*
  - *And your platform threading libraries (non-mandatory)
  - *Does include the C standard library <stdint.h> for uint64_t and uintptr_t, and <stdarg.h> + <stdio.h> for the default `BadAccessGuardReport` function. (easily removed)
- Easy to enable/disable with a single macro: `BAD_ACCESS_GUARDS_ENABLE=0/1`. By default disabled if `defined(NDEBUG)`.
- Battle-tested: Used on projects with 200+ people running the applications/games daily.

# Non-goals/Will not implement
- Detect every single race condition without code change
  - This is not the objective of this library, you would use thread sanitizer, valgrind or other tools for that
  - Some access patterns are not detected on purpose for performance
- Detect lock-free containers issues.
  - This method cannot (unless proven otherwise) work where read/writes need to be considered atomic
- Support every platform in existance.
  - Except for the major ones, we depend on making it easier for the user to add their own rather than implement them all.
- Providing hardened versions of the `std::` containers.
  - If you want this to be supported by `std::` containers, ask your implementer to add this technique to their implementation
  - Perhaps someone could make another repository with wrappers for the `std::` containers ?

# Usage

1. Declare the shadow memory that will hold the state and pointer to stack with `BA_GUARD_DECL(varname)`
2. For all (relevant) read operations of the container/object, use the scope guard `BA_GUARD_READ(varname)`
3. For all (relevant) write operations of the container/object, use the scope guard `BA_GUARD_WRITE(varname)`. But only if it always writes!
4. Add `BA_GUARD_DESTROY(varname)` at the beginning of the destructor
5. Enjoy!

# Examples

Examples are available in [./examples].

# How does it work?

The idea is based on the following observation:

> You do not need to catch all occurences of a race condition, catching it once should be enough when you run the code often and have enough information to locate the issue.

Indeed, (especially in big teams), your code will be run hundred if not thousands of times by your developpers.  
So even if you have 10% of chance to catch the issue with useful details, it is better than 1% chance and not crashing or sometimes way later in the execution of the program. 

Even if detected late, it is better than not being detected at all. And better than having people run sanitizer once every moon eclipse, or only on small unit tests, or worse, never.

## Detection

So our objective is to detect race conditions, as fast as possible. And we do not need 100% detection as long as developpers can afford to run their code with theses tests.

### Access states

We have 3 possible access states for a given object:

- **Idle / Read**: We're either reading the object or not using it at all. These two states are merged because you do not want your reads to be costly!
- **Write**: We're mutating the object. As soon as we are done, we go back to the **Idle** state.
- **Destroy**: The object has been destroyed (or freed), and should not be used anymore.

And there's an implicit one: **Corrupted**, if we see value not in the list above.

### Access states transitions

Now that we have the above states, let's consider the following operations:

- A **Read** is only allowed if the previous state was **Idle / Read** too. Otherwise, the previous operation was not complete.
  - Do not change the state
- Starting a **Write** operation is allowed only if the previous state is **Idle / Read**. 
  - Change state to **Write** 
- After a **Write** operation, we can check that we were still in the **Write** state, then change it back to **Idle / Read**.
- A **Destroy** is allowed only if the previous state is **Idle / Read**.

That's it! Now all we need to do is to check those invariants. If the operations are executed on a single thread, those invariants cannot break.

Which means that if the invariants do not hold, something bad happened!  
There are two options:
- A race condition happened
- Some kind of recursion happened

And since we do not need a 100% error detection, these states changes do not need to be atomic. We just want to force the reads and writes of the state.  
**Remember:** the invariants can not break on a single thread except in the case of a recursion!

> **Note:** We do not care about potential reorders of the operations by the compiler. It must honor the invariants from a single-thread point of view.  
> The re-orders also cannot cause issues (except lowering the detection rate) in a multithread context. If it did, it would mean your synchronization is bad, which you want to detect anyway.  
> The only exception would be for lock-free containers, which are not the target of this library.

## Breaking with more details

The ideal situation would be to have all threads suspended as soon as the race condition occurs.
The next best option is the one we implement: send an interrupt for the debugger to break.

- The sooner we break, the higher your chances of breaking with the other thread at the location responsible for the race condition. 
  - So this one is simple, the first thing we do when detecting a problem is to break! (See `BadAccessGuardConfig::breakASAP`)
  - The faster we can detect issues, the faster we can break. So we limit the cost of the detection.
- If we break, we want to know what thread was involved in the bad access.
  - This is necessary to know if the issue is recursion or a race condition
  - We could have used the thread ID, but this is slow to get. (and again, we want this to be fast). Instead, simply store the stack pointer. This is enough to identify a thread!
    - And if you are using fibers, well, you actually get fiber detection for free too, assuming you keep them around a bit and can list them.

## Keeping it fast and lightweight

We need to store two pieces of information:
- A pointer in the stack of the current thread
- The state.

State will fit in 2 bits, which is lower than the alignment of the stack address and size on virtually every platform.

So we only need to drop the lowest 2 bits of the pointer and store the state there!

> On *Windows*, all stacks are paged aligned (4kB). So we can actually drop the 8 lower bits. This way to get the state the compiler only needs to load a byte instead of masking with `0b11`. 

As for how to obtain our pointer to the current stack... we use intrinsics (see `BA_GUARD_GET_PTR_IN_STACK`), but in theory we could use the address of any variable on the stack (but would then need to make sure the compiler does not optimize it). This is a single `mov` instructions on all platforms.

# Benchmarks

## Setup

- CPU: AMD Ryzen 7 7745HX.
- RAM settings : 5200MT/s, timings 38-38-38-75, 2channels
- CPU was stabilized at a fixed frequency of 3.49Ghz, frequency boost is disabled.

Simple benchmarks were done using `std::vector<>::push_back` since it uses the most expensive guard (write access). 
Memory is reserved upfront, and vector emptied at each iteration, unless ` - noreserve` is mentionned, which means we freed memory after clearing. `complexityN` is the number of elements pushed per iteration.
See [./benchmarks/BenchGuardedVectorExample.cpp](./benchmarks/BenchGuardedVectorExample.cpp).

As usual for microbenchmarks, take them with a grain of salt.

## Release Results

### MSVC 19.42.34436.0 `/EHsc /O2 /Ob1 /DNDEBUG -MD` (CMake Release)

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1 000 |            1 498.09 |          667 517.66 |    0.3% |      1.21 | `std::vector.push_back`
|     100 000 |          146 447.45 |            6 828.39 |    0.3% |      1.21 | `std::vector.push_back`
|  10 000 000 |       14 348 928.57 |               69.69 |    0.3% |      1.19 | `std::vector.push_back`
|       1 000 |            2 293.63 |          435 990.29 |    0.2% |      1.21 | `guardedvector.push_back`
|     100 000 |          226 570.78 |            4 413.63 |    0.1% |      1.21 | `guardedvector.push_back`
|  10 000 000 |       22 766 900.00 |               43.92 |    0.1% |      1.21 | `guardedvector.push_back`
|       1 000 |            3 736.86 |          267 604.40 |    0.0% |      1.22 | `std::vector.push_back - noreserve`
|     100 000 |          574 648.40 |            1 740.19 |    2.8% |      1.18 | `std::vector.push_back - noreserve`
|  10 000 000 |       57 675 900.00 |               17.34 |    0.7% |      1.28 | `std::vector.push_back - noreserve`
|       1 000 |            4 664.21 |          214 398.66 |    0.1% |      1.22 | `guardedvector.push_back - noreserve`
|     100 000 |          619 740.62 |            1 613.58 |    1.0% |      1.20 | `guardedvector.push_back - noreserve`
|  10 000 000 |       70 195 650.00 |               14.25 |    0.8% |      1.20 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1 000 |            6 492.72 |          154 018.73 |    0.0% |      1.21 | `std::vector.push_back`
|     100 000 |          654 289.33 |            1 528.38 |    0.1% |      1.21 | `std::vector.push_back`
|  10 000 000 |       65 633 200.00 |               15.24 |    0.1% |      1.26 | `std::vector.push_back`
|       1 000 |            6 793.21 |          147 205.81 |    0.2% |      1.16 | `guardedvector.push_back`
|     100 000 |          682 157.71 |            1 465.94 |    0.3% |      1.16 | `guardedvector.push_back`
|  10 000 000 |       68 398 600.00 |               14.62 |    0.3% |      1.18 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1 000 |            6 851.06 |          145 962.77 |    0.2% |      1.16 | `std::vector.push_back`
|     100 000 |          690 423.49 |            1 448.39 |    0.3% |      1.16 | `std::vector.push_back`
|  10 000 000 |       69 002 200.00 |               14.49 |    0.0% |      1.07 | `std::vector.push_back`
|       1 000 |            7 100.45 |          140 836.21 |    0.1% |      1.16 | `guardedvector.push_back`
|     100 000 |          708 413.66 |            1 411.60 |    0.3% |      1.17 | `guardedvector.push_back`
|  10 000 000 |       71 019 850.00 |               14.08 |    0.4% |      1.03 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1 000 |            8 266.19 |          120 974.73 |    0.2% |      1.18 | `std::vector.push_back`
|     100 000 |          830 414.73 |            1 204.22 |    0.3% |      1.17 | `std::vector.push_back`
|  10 000 000 |       87 094 900.00 |               11.48 |    0.5% |      0.99 | `std::vector.push_back`
|       1 000 |            8 949.99 |          111 731.94 |    0.2% |      1.18 | `guardedvector.push_back`
|     100 000 |          896 651.75 |            1 115.26 |    0.1% |      1.18 | `guardedvector.push_back`
|  10 000 000 |       93 334 900.00 |               10.71 |    0.6% |      1.07 | `guardedvector.push_back`


## Debug builds results

### MSVC 19.42.34436.0 `/Ob0 /Od /RTC1 -MDd` (CMake Debug default)

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |           54,936.76 |           18,202.75 |    0.8% |      1.21 | `std::vector.push_back`
|       1,000 |           55,363.05 |           18,062.59 |    0.4% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           78,639.80 |           12,716.21 |    0.1% |      1.17 | `guardedvector.push_back`
|       1,000 |           62,009.73 |           16,126.50 |    0.2% |      1.21 | `std::vector.push_back - noreserve`
|       1,000 |           85,885.19 |           11,643.45 |    0.4% |      1.18 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           63,070.99 |           15,855.15 |    0.1% |      1.21 | `std::vector.push_back`
|       1,000 |           65,976.14 |           15,157.00 |    0.1% |      1.16 | `guardedvector.push_back_noguard`
|       1,000 |           90,052.83 |           11,104.59 |    0.1% |      1.18 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           63,697.86 |           15,699.11 |    0.2% |      1.21 | `std::vector.push_back`
|       1,000 |           66,965.40 |           14,933.08 |    0.1% |      1.16 | `guardedvector.push_back_noguard`
|       1,000 |           90,108.84 |           11,097.69 |    0.2% |      1.18 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |          887,023.58 |            1,127.37 |    0.2% |      1.18 | `std::vector.push_back`
|       1,000 |          881,930.77 |            1,133.88 |    0.3% |      1.19 | `guardedvector.push_back_noguard`
|       1,000 |          899,152.07 |            1,112.16 |    0.2% |      1.18 | `guardedvector.push_back`

### MSVC 19.42.34436.0 `/d2Obforceinline /RTC1`

> Note:  NOT `/Od` since it even disables /d2Obforceinline... You should probably be using at least /O1 anyway.

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |           53,764.96 |           18,599.48 |    0.2% |      1.21 | `std::vector.push_back`
|       1,000 |           56,336.35 |           17,750.53 |    0.4% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           72,055.39 |           13,878.21 |    0.3% |      1.17 | `guardedvector.push_back`
|       1,000 |           61,487.42 |           16,263.49 |    0.2% |      1.21 | `std::vector.push_back - noreserve`
|       1,000 |           80,016.81 |           12,497.37 |    0.2% |      1.17 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           64,622.19 |           15,474.56 |    0.2% |      1.21 | `std::vector.push_back`
|       1,000 |           67,344.04 |           14,849.13 |    0.2% |      1.16 | `guardedvector.push_back_noguard`
|       1,000 |           84,253.45 |           11,868.95 |    0.3% |      1.18 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           64,448.62 |           15,516.24 |    0.1% |      1.21 | `std::vector.push_back`
|       1,000 |           67,466.37 |           14,822.20 |    0.3% |      1.16 | `guardedvector.push_back_noguard`
|       1,000 |           83,747.55 |           11,940.65 |    0.2% |      1.18 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |          878,481.54 |            1,138.33 |    0.4% |      1.18 | `std::vector.push_back`
|       1,000 |          881,314.52 |            1,134.67 |    0.2% |      1.18 | `guardedvector.push_back_noguard`
|       1,000 |          892,364.35 |            1,120.62 |    0.2% |      1.18 | `guardedvector.push_back`

### Clang 14.0.6 WSL `-g` (CMake Debug default)

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |           11,488.32 |           87,044.92 |    0.4% |      1.21 | `std::vector.push_back`
|       1,000 |           13,339.45 |           74,965.60 |    0.2% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           20,778.84 |           48,125.87 |    0.5% |      1.21 | `guardedvector.push_back`
|       1,000 |           13,733.73 |           72,813.44 |    0.1% |      1.21 | `std::vector.push_back - noreserve`
|       1,000 |           23,027.56 |           43,426.23 |    0.1% |      1.21 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           18,252.50 |           54,787.02 |    0.0% |      1.21 | `std::vector.push_back`
|       1,000 |           21,287.71 |           46,975.46 |    0.1% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           28,152.29 |           35,521.09 |    0.6% |      1.21 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           19,407.10 |           51,527.54 |    0.1% |      1.21 | `std::vector.push_back`
|       1,000 |           22,420.62 |           44,601.79 |    0.1% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           28,824.75 |           34,692.41 |    0.1% |      1.21 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |           36,769.30 |           27,196.60 |    0.0% |      1.21 | `std::vector.push_back`
|       1,000 |           39,803.70 |           25,123.29 |    0.0% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           45,318.53 |           22,066.03 |    0.1% |      1.21 | `guardedvector.push_back`

### Clang 14.0.6 WSL `-g -Og`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |            1,374.38 |          727,601.59 |    0.1% |      1.21 | `std::vector.push_back`
|       1,000 |            1,489.08 |          671,556.69 |    0.1% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |            4,608.69 |          216,981.55 |    0.2% |      1.21 | `guardedvector.push_back`
|       1,000 |            1,864.62 |          536,302.30 |    0.2% |      1.22 | `std::vector.push_back - noreserve`
|       1,000 |            5,256.88 |          190,226.98 |    0.1% |      1.21 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            6,975.29 |          143,363.30 |    0.1% |      1.16 | `std::vector.push_back`
|       1,000 |            6,995.24 |          142,954.30 |    0.7% |      1.16 | `guardedvector.push_back_noguard`
|       1,000 |            7,237.46 |          138,170.01 |    0.0% |      1.17 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            7,318.30 |          136,643.84 |    0.1% |      1.17 | `std::vector.push_back`
|       1,000 |            7,271.13 |          137,530.17 |    0.1% |      1.17 | `guardedvector.push_back_noguard`
|       1,000 |            7,334.83 |          136,335.86 |    0.1% |      1.17 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |            8,727.46 |          114,580.82 |    0.2% |      1.18 | `std::vector.push_back`
|       1,000 |            8,722.72 |          114,643.08 |    0.1% |      1.18 | `guardedvector.push_back_noguard`
|       1,000 |            9,891.90 |          101,092.79 |    0.1% |      1.19 | `guardedvector.push_back`

## Quick recap

- Release builds (MSVC)
    - vector<uint64_t> => +50% overhead
    - vector<uint64_t> - noreserve => ~10-25% overhead, depends on the growth strategy
    - vector<uint64_t*2> => 5% overhead
    - vector<uint64_t*4> => 3% overhead
    - vector<empty std::string> => 7%overhead
- Debug builds
    - In full debug builds, that is CMake default Debug configuration (MSVC `/Od` / clang without `-Og`)
        - Overhead is between 1% (MSVC std::string)  and 15% (the rest)
        - The base overhead of the debug mode for `std::vector` is so bad anyway (especially for non trivial types), that you might as well just go ahead and use the guards.
        - `push_back_noguard` was added to show the overhead of implementation of the wrapper, which should not be here if you have your own vector implementation.
    - In debug optimized (clang `-g -Og`), results are globally the same as Release builds.

This seems to be a totally acceptable overhead in most cases given the chances it has to detect issues.  
Any object containing the equivalent of two pointers will most likely see only a small decrease in performance for `push_back`.


# LICENSE

This work is released under the [Unlicense](https://unlicense.org/). To sum up, this is in the public domain but you cannot hold contributors liable.
