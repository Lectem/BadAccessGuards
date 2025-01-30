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
|       1,000 |            1,564.15 |          639,323.91 |    0.3% |      1.21 | `std::vector.push_back`
|     100,000 |          154,213.73 |            6,484.51 |    0.2% |      1.21 | `std::vector.push_back`
|  10,000,000 |       15,335,971.43 |               65.21 |    0.4% |      1.22 | `std::vector.push_back`
|       1,000 |            2,299.13 |          434,946.34 |    0.1% |      1.21 | `guardedvector.push_back`
|     100,000 |          229,171.87 |            4,363.54 |    0.2% |      1.21 | `guardedvector.push_back`
|  10,000,000 |       22,935,740.00 |               43.60 |    0.3% |      1.17 | `guardedvector.push_back`
|       1,000 |            3,379.57 |          295,895.42 |    0.1% |      1.20 | `std::vector.push_back - noreserve`
|     100,000 |          714,796.69 |            1,399.00 |    2.5% |      1.10 | `std::vector.push_back - noreserve`
|  10,000,000 |       63,554,500.00 |               15.73 |    0.4% |      1.40 | `std::vector.push_back - noreserve`
|       1,000 |            4,153.50 |          240,760.55 |    0.1% |      1.21 | `guardedvector.push_back - noreserve`
|     100,000 |          613,052.66 |            1,631.18 |    4.0% |      1.07 | `guardedvector.push_back - noreserve`
|  10,000,000 |       76,297,700.00 |               13.11 |    0.4% |      1.06 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            6,551.51 |          152,636.61 |    0.2% |      1.21 | `std::vector.push_back`
|     100,000 |          658,357.05 |            1,518.93 |    0.2% |      1.21 | `std::vector.push_back`
|  10,000,000 |       66,165,800.00 |               15.11 |    0.4% |      1.27 | `std::vector.push_back`
|       1,000 |            6,826.71 |          146,483.42 |    0.1% |      1.17 | `guardedvector.push_back`
|     100,000 |          681,602.38 |            1,467.13 |    0.2% |      1.16 | `guardedvector.push_back`
|  10,000,000 |       68,201,950.00 |               14.66 |    0.2% |      1.18 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            7,158.25 |          139,698.96 |    0.2% |      1.16 | `std::vector.push_back`
|     100,000 |          719,018.42 |            1,390.78 |    0.2% |      1.17 | `std::vector.push_back`
|  10,000,000 |       72,046,100.00 |               13.88 |    0.1% |      1.04 | `std::vector.push_back`
|       1,000 |            7,111.63 |          140,614.74 |    0.3% |      1.16 | `guardedvector.push_back`
|     100,000 |          714,421.74 |            1,399.73 |    0.2% |      1.17 | `guardedvector.push_back`
|  10,000,000 |       71,789,000.00 |               13.93 |    0.3% |      1.04 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |            8,467.38 |          118,100.24 |    0.2% |      1.18 | `std::vector.push_back`
|     100,000 |          853,885.93 |            1,171.12 |    0.1% |      1.18 | `std::vector.push_back`
|  10,000,000 |       89,772,500.00 |               11.14 |    0.3% |      1.02 | `std::vector.push_back`
|       1,000 |            9,313.27 |          107,373.67 |    0.1% |      1.19 | `guardedvector.push_back`
|     100,000 |          931,452.34 |            1,073.59 |    0.3% |      1.18 | `guardedvector.push_back`
|  10,000,000 |       97,508,400.00 |               10.26 |    0.4% |      1.12 | `guardedvector.push_back`

### Clang 14.0.6 WSL `-O3 -DNDEBUG` (CMake Release default)

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |            1,255.43 |          796,539.90 |    0.3% |      1.21 | `std::vector.push_back`
|     100,000 |          125,474.04 |            7,969.78 |    0.1% |      1.21 | `std::vector.push_back`
|  10,000,000 |       12,602,706.56 |               79.35 |    0.3% |      1.21 | `std::vector.push_back`
|       1,000 |            5,177.10 |          193,158.21 |    0.0% |      1.21 | `guardedvector.push_back`
|     100,000 |          521,834.49 |            1,916.32 |    0.5% |      1.22 | `guardedvector.push_back`
|  10,000,000 |       52,530,619.00 |               19.04 |    0.4% |      1.07 | `guardedvector.push_back`
|       1,000 |            1,793.98 |          557,420.87 |    0.1% |      1.21 | `std::vector.push_back - noreserve`
|     100,000 |          789,574.04 |            1,266.51 |    0.2% |      1.18 | `std::vector.push_back - noreserve`
|  10,000,000 |       41,802,090.00 |               23.92 |    0.7% |      1.26 | `std::vector.push_back - noreserve`
|       1,000 |            5,749.73 |          173,921.34 |    0.1% |      1.21 | `guardedvector.push_back - noreserve`
|     100,000 |          561,216.09 |            1,781.84 |    0.0% |      1.21 | `guardedvector.push_back - noreserve`
|  10,000,000 |       81,652,733.00 |               12.25 |    0.7% |      0.92 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            1,510.51 |          662,029.30 |    0.1% |      1.21 | `std::vector.push_back`
|     100,000 |          151,351.00 |            6,607.16 |    0.1% |      1.21 | `std::vector.push_back`
|  10,000,000 |       15,283,910.14 |               65.43 |    0.1% |      1.25 | `std::vector.push_back`
|       1,000 |            3,470.75 |          288,122.19 |    0.1% |      1.21 | `guardedvector.push_back`
|     100,000 |          344,128.11 |            2,905.89 |    0.1% |      1.22 | `guardedvector.push_back`
|  10,000,000 |       34,999,904.00 |               28.57 |    0.2% |      1.05 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            1,782.07 |          561,144.07 |    0.1% |      1.21 | `std::vector.push_back`
|     100,000 |          174,452.56 |            5,732.22 |    0.4% |      1.22 | `std::vector.push_back`
|  10,000,000 |       19,031,163.40 |               52.55 |    0.5% |      0.84 | `std::vector.push_back`
|       1,000 |            3,536.04 |          282,801.98 |    0.1% |      1.21 | `guardedvector.push_back`
|     100,000 |          349,298.37 |            2,862.88 |    0.6% |      1.21 | `guardedvector.push_back`
|  10,000,000 |       35,701,763.00 |               28.01 |    0.1% |      0.99 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |            8,251.14 |          121,195.32 |    0.1% |      1.18 | `std::vector.push_back`
|     100,000 |          801,689.65 |            1,247.37 |    0.2% |      1.18 | `std::vector.push_back`
|  10,000,000 |       85,695,325.00 |               11.67 |    0.3% |      1.00 | `std::vector.push_back`
|       1,000 |            8,541.92 |          117,069.75 |    0.0% |      1.18 | `guardedvector.push_back`
|     100,000 |          837,175.80 |            1,194.49 |    0.1% |      1.19 | `guardedvector.push_back`
|  10,000,000 |       89,646,819.00 |               11.15 |    0.9% |      1.04 | `guardedvector.push_back`

### Clang 14.0.6 WSL `-O2 -DNDEBUG` (CMake RelWithDebInfo default)
| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |            1,268.95 |          788,051.74 |    0.1% |      1.21 | `std::vector.push_back`
|     100,000 |          126,408.23 |            7,910.88 |    0.1% |      1.21 | `std::vector.push_back`
|  10,000,000 |       12,687,909.22 |               78.82 |    0.1% |      1.19 | `std::vector.push_back`
|       1,000 |            4,583.13 |          218,191.46 |    0.1% |      1.21 | `guardedvector.push_back`
|     100,000 |          461,465.16 |            2,167.01 |    0.5% |      1.22 | `guardedvector.push_back`
|  10,000,000 |       45,963,623.50 |               21.76 |    0.1% |      1.19 | `guardedvector.push_back`
|       1,000 |            1,830.49 |          546,302.77 |    0.1% |      1.21 | `std::vector.push_back - noreserve`
|     100,000 |          792,822.74 |            1,261.32 |    0.3% |      1.17 | `std::vector.push_back - noreserve`
|  10,000,000 |       42,369,383.67 |               23.60 |    1.2% |      1.23 | `std::vector.push_back - noreserve`
|       1,000 |            5,216.11 |          191,713.57 |    0.7% |      1.21 | `guardedvector.push_back - noreserve`
|     100,000 |          501,444.59 |            1,994.24 |    0.1% |      1.18 | `guardedvector.push_back - noreserve`
|  10,000,000 |       75,929,147.00 |               13.17 |    0.9% |      0.93 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            6,950.17 |          143,881.46 |    0.2% |      1.18 | `std::vector.push_back`
|     100,000 |          702,932.35 |            1,422.61 |    0.4% |      1.17 | `std::vector.push_back`
|  10,000,000 |       70,359,581.00 |               14.21 |    0.3% |      1.02 | `std::vector.push_back`
|       1,000 |            7,043.86 |          141,967.58 |    0.2% |      1.17 | `guardedvector.push_back`
|     100,000 |          700,146.79 |            1,428.27 |    0.1% |      1.16 | `guardedvector.push_back`
|  10,000,000 |       70,189,106.00 |               14.25 |    0.3% |      1.02 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            7,258.91 |          137,761.67 |    0.2% |      1.17 | `std::vector.push_back`
|     100,000 |          725,305.81 |            1,378.73 |    0.1% |      1.16 | `std::vector.push_back`
|  10,000,000 |       73,063,461.00 |               13.69 |    0.1% |      0.87 | `std::vector.push_back`
|       1,000 |            8,186.97 |          122,145.24 |    0.0% |      1.18 | `guardedvector.push_back`
|     100,000 |          751,092.72 |            1,331.39 |    0.1% |      1.19 | `guardedvector.push_back`
|  10,000,000 |       83,006,817.00 |               12.05 |    0.4% |      0.97 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |            9,304.15 |          107,478.92 |    0.4% |      1.19 | `std::vector.push_back`
|     100,000 |          936,314.02 |            1,068.02 |    0.3% |      1.19 | `std::vector.push_back`
|  10,000,000 |       95,620,062.00 |               10.46 |    0.2% |      1.11 | `std::vector.push_back`
|       1,000 |           10,389.77 |           96,248.51 |    0.1% |      1.21 | `guardedvector.push_back`
|     100,000 |        1,039,111.34 |              962.36 |    0.1% |      1.21 | `guardedvector.push_back`
|  10,000,000 |      108,038,449.00 |                9.26 |    0.4% |      1.25 | `guardedvector.push_back`

### GCC 12.2.0 WSL `-O3 -DNDEBUG` (CMake Release default)

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |              914.55 |        1,093,437.87 |    0.1% |      1.19 | `std::vector.push_back`
|     100,000 |           91,657.94 |           10,910.13 |    0.3% |      1.19 | `std::vector.push_back`
|  10,000,000 |        9,405,401.83 |              106.32 |    0.3% |      1.20 | `std::vector.push_back`
|       1,000 |            2,298.95 |          434,980.49 |    0.1% |      1.21 | `guardedvector.push_back`
|     100,000 |          229,764.35 |            4,352.29 |    0.1% |      1.21 | `guardedvector.push_back`
|  10,000,000 |       23,035,121.25 |               43.41 |    0.1% |      1.17 | `guardedvector.push_back`
|       1,000 |            1,491.71 |          670,371.51 |    0.0% |      1.21 | `std::vector.push_back - noreserve`
|     100,000 |          749,281.29 |            1,334.61 |    0.6% |      1.18 | `std::vector.push_back - noreserve`
|  10,000,000 |       37,198,530.67 |               26.88 |    1.6% |      1.18 | `std::vector.push_back - noreserve`
|       1,000 |            2,929.68 |          341,334.35 |    0.3% |      1.22 | `guardedvector.push_back - noreserve`
|     100,000 |          275,802.73 |            3,625.78 |    0.2% |      1.21 | `guardedvector.push_back - noreserve`
|  10,000,000 |       51,219,745.50 |               19.52 |    1.6% |      1.08 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            6,582.44 |          151,919.41 |    0.1% |      1.21 | `std::vector.push_back`
|     100,000 |          660,532.34 |            1,513.93 |    0.2% |      1.21 | `std::vector.push_back`
|  10,000,000 |       66,092,501.00 |               15.13 |    0.6% |      1.16 | `std::vector.push_back`
|       1,000 |            6,867.22 |          145,619.32 |    0.4% |      1.16 | `guardedvector.push_back`
|     100,000 |          689,774.62 |            1,449.75 |    0.1% |      1.17 | `guardedvector.push_back`
|  10,000,000 |       69,074,072.50 |               14.48 |    0.2% |      1.07 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            8,986.54 |          111,277.49 |    0.2% |      1.18 | `std::vector.push_back`
|     100,000 |          776,507.54 |            1,287.82 |    0.4% |      1.18 | `std::vector.push_back`
|  10,000,000 |       90,180,989.00 |               11.09 |    0.2% |      1.05 | `std::vector.push_back`
|       1,000 |            7,884.75 |          126,827.11 |    0.2% |      1.17 | `guardedvector.push_back`
|     100,000 |          753,485.57 |            1,327.17 |    0.1% |      1.17 | `guardedvector.push_back`
|  10,000,000 |       80,594,003.00 |               12.41 |    0.1% |      0.95 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |            9,321.70 |          107,276.60 |    0.1% |      1.19 | `std::vector.push_back`
|     100,000 |          776,655.69 |            1,287.57 |    0.3% |      1.18 | `std::vector.push_back`
|  10,000,000 |       97,298,103.00 |               10.28 |    0.8% |      1.13 | `std::vector.push_back`
|       1,000 |            9,013.14 |          110,949.10 |    0.0% |      1.18 | `guardedvector.push_back`
|     100,000 |          875,499.57 |            1,142.21 |    0.4% |      1.18 | `guardedvector.push_back`
|  10,000,000 |       92,305,430.00 |               10.83 |    0.7% |      1.08 | `guardedvector.push_back`


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
    - vector<std::string of 1 char> => 5-7%overhead
    - clang Release (-O3) seems to be the exception and shows a much bigger overhead for all types except std::string
        - It seems the guards defeat some kind of optimization here
        - Did not investigate why yet
- Debug builds
    - In CMake Debug default configuration (MSVC `/Od` / clang without `-Og`)
        - Overhead is between 1% (MSVC std::string)  and 15% (the rest)
        - The base overhead of the debug mode for `std::vector` is so bad anyway (especially for non trivial types), that you might as well just go ahead and use the guards.
        - `push_back_noguard` was added to show the overhead of implementation of the wrapper, which should not be here if you have your own vector implementation.
    - In debug optimized (clang `-g -Og`), results are globally the same as Release builds.

This seems to be a totally acceptable overhead in most cases given the chances it has to detect issues.  
Any object containing the equivalent of two pointers will most likely see only a small decrease in performance for `push_back`.
However, I would still recommand disabling the guards in production.


# LICENSE

This work is released under the [Unlicense](https://unlicense.org/). To sum up, this is in the public domain but you cannot hold contributors liable.
