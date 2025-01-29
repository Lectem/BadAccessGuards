include(FetchContent)

FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG e4327893194f06928012eb81cabc606c4e4791ac
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(nanobench)

add_executable(BenchGuardedVectorExample BenchGuardedVectorExample.cpp)
target_link_libraries(BenchGuardedVectorExample 
    PRIVATE
        BadAccessGuards
        nanobench
)

