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
option(BAGUARDS_FORCE_ENABLE "Build with BAD_ACCESS_GUARDS_ENABLE=1 defined." ${BAGUARDS_IS_ROOT_PROJECT})


add_library(BadAccessGuards 
	src/BadAccessGuards.cpp
	src/BadAccessGuards.h
)
target_include_directories(BadAccessGuards INTERFACE src)
target_compile_features(BadAccessGuards PUBLIC cxx_std_11)

if(BAGUARDS_FORCE_ENABLE)
	target_compile_definitions(BadAccessGuards PUBLIC BAD_ACCESS_GUARDS_ENABLE=1)
endif()

if(BAGUARDS_EXAMPLES)
	add_executable(BasicExample examples/Basic.cpp)
	target_link_libraries(BasicExample PRIVATE BadAccessGuards)

	add_executable(GuardedVectorExample examples/GuardedVectorExample.cpp examples/GuardedVectorExample.h)
	target_link_libraries(GuardedVectorExample PRIVATE BadAccessGuards)
endif()

if(BAGUARDS_BENCH)
	add_subdirectory(benchmarks)
endif()

