cmake_minimum_required(VERSION 3.25)

project(BadAccessGuards C CXX)

add_library(BadAccessGuards 
	src/BadAccessGuards.cpp
	src/BadAccessGuards.h
)
target_include_directories(BadAccessGuards INTERFACE src)

add_executable(BasicExample examples/Basic.cpp)
target_link_libraries(BasicExample PRIVATE BadAccessGuards)

add_executable(GuardedVectorExample examples/GuardedVectorExample.cpp examples/GuardedVectorExample.h)
target_link_libraries(GuardedVectorExample PRIVATE BadAccessGuards)

