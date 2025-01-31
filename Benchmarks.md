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

## Summary

- Release builds
    - vector<uint64_t> => +50% overhead
    - vector<uint64_t> - noreserve => ~10-25% overhead, depends on the growth strategy
    - vector<uint64_t*2> => 5% overhead
    - vector<uint64_t*4> => 3% overhead
    - vector<std::string of 1 char> => 5-7%overhead
    - clang Release `-O3` (but not `-O2`) seems to be the exception and shows a much bigger overhead for all types except `std::string`
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
