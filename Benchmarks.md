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
|       1,000 |           53,578.34 |           18,664.26 |    0.1% |      1.21 | `std::vector.push_back`
|       1,000 |           54,655.18 |           18,296.53 |    0.2% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           78,635.86 |           12,716.84 |    0.4% |      1.17 | `guardedvector.push_back`
|       1,000 |           61,116.90 |           16,362.09 |    0.1% |      1.20 | `std::vector.push_back - noreserve`
|       1,000 |           62,312.97 |           16,048.02 |    0.1% |      1.21 | `guardedvector.push_back_noguard - noreserve`
|       1,000 |           85,988.50 |           11,629.46 |    0.0% |      1.18 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           63,358.73 |           15,783.14 |    0.0% |      1.21 | `std::vector.push_back`
|       1,000 |           66,385.34 |           15,063.57 |    0.0% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           90,441.90 |           11,056.82 |    0.1% |      1.18 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           62,788.88 |           15,926.39 |    0.1% |      1.21 | `std::vector.push_back`
|       1,000 |           65,782.26 |           15,201.67 |    0.0% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           89,670.20 |           11,151.98 |    0.1% |      1.18 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |          892,186.67 |            1,120.84 |    0.0% |      1.18 | `std::vector.push_back`
|       1,000 |          891,737.39 |            1,121.41 |    0.0% |      1.18 | `guardedvector.push_back_noguard`
|       1,000 |          921,326.50 |            1,085.39 |    0.1% |      1.19 | `guardedvector.push_back`

### MSVC 19.42.34436.0 `/d2Obforceinline /RTC1`

> Note:  NOT `/Od` since it even disables `/d2Obforceinline`... You should probably be using at least `/O1` anyway.

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |           55,695.84 |           17,954.66 |    0.3% |      1.21 | `std::vector.push_back`
|       1,000 |           55,816.60 |           17,915.82 |    0.2% |      1.22 | `guardedvector.push_back_noguard`
|       1,000 |           71,371.23 |           14,011.25 |    0.3% |      1.16 | `guardedvector.push_back`
|       1,000 |           63,074.18 |           15,854.35 |    0.2% |      1.21 | `std::vector.push_back - noreserve`
|       1,000 |           63,063.28 |           15,857.09 |    0.1% |      1.21 | `guardedvector.push_back_noguard - noreserve`
|       1,000 |           78,940.42 |           12,667.78 |    0.1% |      1.17 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           63,507.71 |           15,746.12 |    0.1% |      1.21 | `std::vector.push_back`
|       1,000 |           66,471.69 |           15,044.00 |    0.1% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           83,548.44 |           11,969.10 |    0.1% |      1.18 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           63,555.23 |           15,734.35 |    0.2% |      1.21 | `std::vector.push_back`
|       1,000 |           66,519.09 |           15,033.28 |    0.1% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           82,997.72 |           12,048.52 |    0.1% |      1.18 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |          881,261.21 |            1,134.74 |    0.0% |      1.18 | `std::vector.push_back`
|       1,000 |          888,316.95 |            1,125.72 |    0.1% |      1.18 | `guardedvector.push_back_noguard`
|       1,000 |          898,869.30 |            1,112.51 |    0.1% |      1.18 | `guardedvector.push_back`

### Clang 14.0.6 WSL `-g` (CMake Debug default)

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |           11,448.58 |           87,347.04 |    0.1% |      1.21 | `std::vector.push_back`
|       1,000 |           13,378.30 |           74,747.89 |    1.1% |      1.22 | `guardedvector.push_back_noguard`
|       1,000 |           20,614.45 |           48,509.65 |    0.1% |      1.21 | `guardedvector.push_back`
|       1,000 |           13,890.99 |           71,989.10 |    0.1% |      1.21 | `std::vector.push_back - noreserve`
|       1,000 |           16,102.80 |           62,101.00 |    0.1% |      1.21 | `guardedvector.push_back_noguard - noreserve`
|       1,000 |           23,119.37 |           43,253.76 |    0.1% |      1.21 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           18,230.03 |           54,854.56 |    0.0% |      1.21 | `std::vector.push_back`
|       1,000 |           21,258.31 |           47,040.43 |    0.0% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           27,903.21 |           35,838.17 |    0.0% |      1.21 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           19,238.49 |           51,979.13 |    0.0% |      1.21 | `std::vector.push_back`
|       1,000 |           22,229.66 |           44,984.93 |    0.1% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |           28,778.67 |           34,747.96 |    0.0% |      1.21 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |           40,453.30 |           24,719.86 |    0.0% |      1.21 | `std::vector.push_back`
|       1,000 |           43,442.36 |           23,019.01 |    0.1% |      1.22 | `guardedvector.push_back_noguard`
|       1,000 |           49,608.16 |           20,157.97 |    0.2% |      1.22 | `guardedvector.push_back`

### Clang 14.0.6 WSL `-g -Og`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |            1,370.84 |          729,480.18 |    0.1% |      1.21 | `std::vector.push_back`
|       1,000 |            1,493.93 |          669,375.85 |    0.0% |      1.21 | `guardedvector.push_back_noguard`
|       1,000 |            4,596.86 |          217,539.95 |    0.0% |      1.21 | `guardedvector.push_back`
|       1,000 |            1,871.19 |          534,419.24 |    0.1% |      1.23 | `std::vector.push_back - noreserve`
|       1,000 |            2,010.68 |          497,343.61 |    0.0% |      1.22 | `guardedvector.push_back_noguard - noreserve`
|       1,000 |            5,165.98 |          193,574.13 |    0.1% |      1.21 | `guardedvector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            6,907.74 |          144,765.06 |    0.0% |      1.16 | `std::vector.push_back`
|       1,000 |            7,228.74 |          138,336.76 |    0.1% |      1.17 | `guardedvector.push_back_noguard`
|       1,000 |            7,243.88 |          138,047.62 |    0.1% |      1.17 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |            7,296.97 |          137,043.15 |    0.0% |      1.17 | `std::vector.push_back`
|       1,000 |            7,242.02 |          138,082.96 |    0.1% |      1.17 | `guardedvector.push_back_noguard`
|       1,000 |            7,359.22 |          135,883.91 |    0.0% |      1.17 | `guardedvector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |            9,203.12 |          108,658.80 |    0.0% |      1.19 | `std::vector.push_back`
|       1,000 |            9,208.55 |          108,594.77 |    0.0% |      1.19 | `guardedvector.push_back_noguard`
|       1,000 |           10,355.30 |           96,568.95 |    0.0% |      1.21 | `guardedvector.push_back`

## ThreadSanitizer

### Clang 14.0.6 WSL `-fsanitize=thread -O3 -DNDEBUG` (CMake Release default)

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |           27,569.43 |           36,272.06 |    0.1% |      1.21 | `std::vector.push_back`
|     100,000 |        2,755,273.56 |              362.94 |    0.1% |      1.21 | `std::vector.push_back`
|  10,000,000 |      275,502,920.00 |                3.63 |    0.0% |      3.18 | `std::vector.push_back`
|       1,000 |           34,961.04 |           28,603.27 |    0.0% |      1.21 | `std::vector.push_back - noreserve`
|     100,000 |        6,557,961.12 |              152.49 |    0.1% |      1.22 | `std::vector.push_back - noreserve`
|  10,000,000 |      873,837,301.00 |                1.14 |    3.2% |      9.30 | `std::vector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           33,820.31 |           29,568.03 |    0.1% |      1.22 | `std::vector.push_back`
|     100,000 |        3,385,774.65 |              295.35 |    0.1% |      1.21 | `std::vector.push_back`
|  10,000,000 |      338,732,175.00 |                2.95 |    0.1% |      4.15 | `std::vector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           54,151.45 |           18,466.73 |    0.0% |      1.21 | `std::vector.push_back`
|     100,000 |        5,437,534.41 |              183.91 |    0.2% |      1.22 | `std::vector.push_back`
|  10,000,000 |      543,613,425.00 |                1.84 |    0.1% |      6.57 | `std::vector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |           48,015.64 |           20,826.55 |    0.1% |      1.21 | `std::vector.push_back`
|     100,000 |        4,812,346.95 |              207.80 |    0.1% |      1.23 | `std::vector.push_back`
|  10,000,000 |      483,697,656.00 |                2.07 |    0.1% |      5.90 | `std::vector.push_back`


### Clang 14.0.6 WSL `-fsanitize=thread -O2 -DNDEBUG` (CMake RelWithDebInfo default)


| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t
|------------:|--------------------:|--------------------:|--------:|----------:|:-------------------
|       1,000 |           27,600.31 |           36,231.48 |    0.1% |      1.21 | `std::vector.push_back`
|     100,000 |        2,756,755.84 |              362.75 |    0.1% |      1.21 | `std::vector.push_back`
|  10,000,000 |      275,798,601.00 |                3.63 |    0.1% |      3.18 | `std::vector.push_back`
|       1,000 |           35,054.95 |           28,526.64 |    0.0% |      1.21 | `std::vector.push_back - noreserve`
|     100,000 |        7,808,870.40 |              128.06 |    0.2% |      1.18 | `std::vector.push_back - noreserve`
|  10,000,000 |      888,449,152.00 |                1.13 |    1.1% |      9.83 | `std::vector.push_back - noreserve`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*2
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           65,370.03 |           15,297.53 |    0.1% |      1.16 | `std::vector.push_back`
|     100,000 |        6,549,550.81 |              152.68 |    0.2% |      1.14 | `std::vector.push_back`
|  10,000,000 |      653,770,052.00 |                1.53 |    0.0% |      7.47 | `std::vector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of uint64_t*4
|------------:|--------------------:|--------------------:|--------:|----------:|:---------------------
|       1,000 |           91,220.54 |           10,962.44 |    0.2% |      1.19 | `std::vector.push_back`
|     100,000 |        9,141,664.85 |              109.39 |    0.1% |      1.23 | `std::vector.push_back`
|  10,000,000 |      914,898,255.00 |                1.09 |    0.1% |     10.89 | `std::vector.push_back`

| complexityN |               ns/op |                op/s |    err% |     total | Vector of std::string
|------------:|--------------------:|--------------------:|--------:|----------:|:----------------------
|       1,000 |           91,597.87 |           10,917.28 |    0.0% |      1.19 | `std::vector.push_back`
|     100,000 |        9,182,358.45 |              108.90 |    0.1% |      1.23 | `std::vector.push_back`
|  10,000,000 |      920,280,188.00 |                1.09 |    0.1% |     10.69 | `std::vector.push_back`


## Summary

- Release builds
    - `vector<uint64_t>` => **+50%** overhead
    - `vector<uint64_t>` - noreserve => **~10-25%** overhead, depends on the growth strategy
    - `vector<uint64_t*2>` => **5%** overhead
    - `vector<uint64_t*4>` => **3%** overhead
    - `vector<std::string of 1 char>` => **5-7%** overhead
    - clang Release `-O3` (but not `-O2` nor GCC `-O3`) seems to be the exception and shows a much bigger overhead (between **+100%** and **+150%**) for types other than `std::string`
        - It seems the guards defeat some kind of optimization here
        - Did not investigate why yet
- Debug builds
    - In CMake Debug default configuration (MSVC `/Od` / clang without `-Og`)
        - Overhead is between 1% (`std::string`)  and **30%** (the rest)
        - The base overhead of the debug mode for `std::vector` is so bad anyway (especially for non-trivial types), that you might as well just go ahead and use the guards.
        - `push_back_noguard` was added to show the overhead of the implementation of the wrapper, which should not be here if you have your own vector implementation.
    - In debug optimized (clang `-Og`), results are globally the same as Release builds.

This seems to be a totally acceptable overhead in most cases given the chances it has to detect issues.  
Any object containing the equivalent of two pointers will most likely see only a small decrease in performance for `push_back`.

Compared to thread sanitizer, this is 5 to 30 times faster. Of course the detection level is way lower, but this is great as a smoketest since your code runs a lot during development.

On games for which we tested the guards, less than 2% of regression in frame duration was observed. Which makes sense, since you do not (rather, should not) spend most of your time doing operations on containers.

However, I would still recommend disabling the guards in production.
