cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source.")
endif()

project(BadAccessGuards LANGUAGES C CXX VERSION 1.0.0)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(${PROJECT_NAME}_IS_ROOT_PROJECT TRUE)
endif()

############################
##  Modules and scripts   ##
############################

include(GNUInstallDirs)                 # This will define the default values for installation directories (all platforms even if named GNU)
#include(InstallRequiredSystemLibraries) # Tell CMake that the `install` target needs to install required system libraries (eg: Windows SDK)
include(CMakePackageConfigHelpers)      # Helper to create relocatable packages

###############
##  OPTIONS  ##
###############

option(${PROJECT_NAME}_EXAMPLES "Build the examples" ${${PROJECT_NAME}_IS_ROOT_PROJECT})
option(${PROJECT_NAME}_BENCH "Build the benchmarks" ${${PROJECT_NAME}_IS_ROOT_PROJECT})
option(${PROJECT_NAME}_FORCE_ENABLE "Build with BAD_ACCESS_GUARDS_ENABLE=1 defined." ${${PROJECT_NAME}_IS_ROOT_PROJECT})
option(${PROJECT_NAME}_INSTALL "Should ${PROJECT_NAME} be added to the install list? Useful if included using add_subdirectory." ${${PROJECT_NAME}_IS_ROOT_PROJECT})

###############
##  PROJECT  ##
###############

add_library(BadAccessGuards 
	src/BadAccessGuards.cpp
	src/BadAccessGuards.h
)
target_include_directories(${PROJECT_NAME} 
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src> # Due to the way installation work, we only want this path set when building, not once installed
)
set_target_properties(${PROJECT_NAME} 
    PROPERTIES 
        PUBLIC_HEADER ${CMAKE_CURRENT_LIST_DIR}/src/BadAccessGuards.h
        DEBUG_POSTFIX d
)

target_compile_features(BadAccessGuards PUBLIC cxx_std_11)
add_library(${PROJECT_NAME}::BadAccessGuards ALIAS BadAccessGuards)

if(${PROJECT_NAME}_FORCE_ENABLE)
	target_compile_definitions(BadAccessGuards PUBLIC BAD_ACCESS_GUARDS_ENABLE=1)
endif()

#############################
## Examples and benchmarks ##
#############################

if(${PROJECT_NAME}_EXAMPLES)
	add_executable(BasicExample examples/Basic.cpp)
	target_link_libraries(BasicExample PRIVATE BadAccessGuards)
	target_compile_features(BasicExample PUBLIC cxx_std_14) # chrono_literals
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT BasicExample)

	add_executable(GuardedVectorExample examples/GuardedVectorExample.cpp examples/GuardedVectorExample.h)
	target_link_libraries(GuardedVectorExample PRIVATE BadAccessGuards)
	target_compile_features(GuardedVectorExample PUBLIC cxx_std_14) # chrono_literals
endif()

if(${PROJECT_NAME}_BENCH)
	add_subdirectory(benchmarks)
endif()

#############
## Install ##
#############

if(${PROJECT_NAME}_INSTALL)
	set(${PROJECT_NAME}_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "Path to ${PROJECT_NAME} cmake files")

	# Use version checking boilerplate
	write_basic_package_version_file(
		${PROJECT_NAME}ConfigVersion.cmake
		COMPATIBILITY SameMajorVersion
	)

	configure_package_config_file(
		${CMAKE_CURRENT_LIST_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
		${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
		INSTALL_DESTINATION ${${PROJECT_NAME}_INSTALL_CMAKEDIR} 
		# Imported targets do not require the following macros
		NO_SET_AND_CHECK_MACRO
		NO_CHECK_REQUIRED_COMPONENTS_MACRO 
	)
	
	install(
		TARGETS BadAccessGuards
		EXPORT ${PROJECT_NAME}_Targets
		INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	)

	install(
		EXPORT ${PROJECT_NAME}_Targets
		NAMESPACE ${PROJECT_NAME}::
		FILE ${PROJECT_NAME}Targets.cmake
		DESTINATION ${${PROJECT_NAME}_INSTALL_CMAKEDIR}
	)

	install(FILES 
		${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
		${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
		DESTINATION ${${PROJECT_NAME}_INSTALL_CMAKEDIR}
	)
endif()

