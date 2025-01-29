cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source.")
endif()

project(BadAccessGuards C CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(BAGUARDS_IS_ROOT_PROJECT TRUE)
endif()

option(BAGUARDS_EXAMPLES "Build the examples" ${BAGUARDS_IS_ROOT_PROJECT})
option(BAGUARDS_BENCH "Build the benchmarks" ${BAGUARDS_IS_ROOT_PROJECT})


add_library(BadAccessGuards 
	src/BadAccessGuards.cpp
	src/BadAccessGuards.h
)
target_include_directories(BadAccessGuards INTERFACE src)

if(BAGUARDS_EXAMPLES)
	add_executable(BasicExample examples/Basic.cpp)
	target_link_libraries(BasicExample PRIVATE BadAccessGuards)

	add_executable(GuardedVectorExample examples/GuardedVectorExample.cpp examples/GuardedVectorExample.h)
	target_link_libraries(GuardedVectorExample PRIVATE BadAccessGuards)
endif()

if(BAGUARDS_BENCH)
	add_subdirectory(benchmarks)
endif()

