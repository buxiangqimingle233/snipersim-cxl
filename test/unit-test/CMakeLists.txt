cmake_minimum_required(VERSION 3.10)

# Set the project name
project(TestProject)

# Find the UnitTest++ package
find_package(UnitTest++ REQUIRED)

# Include directories
include_directories(${UTPP_INCLUDE_DIRS})


# Recursively include all directories under common
file(GLOB_RECURSE all_include_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/../../common/*")
foreach(dir ${all_include_dirs})
  if(IS_DIRECTORY ${dir})
    include_directories(${dir})
  endif()
endforeach()


# Rebuilds the carbon_sim library with the UNIT_TEST flag set
# execute_process(COMMAND make clean
#                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
#                 RESULT_VARIABLE command_result)
# if(NOT command_result EQUAL 0)
#   message(FATAL_ERROR "Command 'make clean' failed with error code: ${command_result}")
# endif()

# execute_process(COMMAND make RECORD_CXL_TRACE=1 -j
#                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
#                 RESULT_VARIABLE command_result)
# if(NOT command_result EQUAL 0)
#   message(FATAL_ERROR "Command 'make RECORD_CXL_TRACE=1 -j' failed with error code: ${command_result}")
# endif()

# Recompile the object that we want to test
add_definitions(-DUNIT_TEST=1)
file(GLOB_RECURSE all_source_files 
  "${CMAKE_CURRENT_SOURCE_DIR}/../../common/performance_model/cxtnl_shim/*.cc")
add_library(ep_agent STATIC ${all_source_files})

# Add the test executable
add_executable(test1 unit_test.cc)

# Add debug flags to the targets
target_compile_options(ep_agent PRIVATE -g -O0)
target_compile_options(test1 PRIVATE -g -O0)

# Link the test executable with the library
target_link_libraries(test1 UnitTest++ ep_agent)
