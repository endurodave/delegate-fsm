# Example CMake command line to create project build files:
#
# cmake -B Build .

cmake_minimum_required(VERSION 3.10)

project(StateMachine VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# DelegateMQ build options
set(DMQ_ALLOCATOR "OFF")
set(DMQ_UTIL "ON")
set(DMQ_THREAD "DMQ_THREAD_STDLIB")
set(DMQ_SERIALIZE "DMQ_SERIALIZE_NONE")
set(DMQ_TRANSPORT "DMQ_TRANSPORT_NONE")
include("${CMAKE_SOURCE_DIR}/delegate-mq/DelegateMQ.cmake")

# Collect project source files
file(GLOB SOURCES      "${CMAKE_SOURCE_DIR}/main.cpp")
file(GLOB SM_SOURCES   "${CMAKE_SOURCE_DIR}/state-machine/*.cpp"
                       "${CMAKE_SOURCE_DIR}/state-machine/*.h")
file(GLOB EXAMPLE_SOURCES "${CMAKE_SOURCE_DIR}/examples/*.cpp"
                          "${CMAKE_SOURCE_DIR}/examples/*.h")
file(GLOB TEST_SOURCES "${CMAKE_SOURCE_DIR}/unit-tests/*.cpp"
                       "${CMAKE_SOURCE_DIR}/unit-tests/*.h")

list(APPEND SOURCES ${SM_SOURCES} ${EXAMPLE_SOURCES} ${TEST_SOURCES} ${DMQ_PREDEF_SOURCES})

# Organize files in IDE (Visual Studio Solution Explorer)
source_group("State Machine"   FILES ${SM_SOURCES})
source_group("Examples"        FILES ${EXAMPLE_SOURCES})
source_group("Unit Tests"      FILES ${TEST_SOURCES})
source_group("Delegate Files"  FILES ${DMQ_LIB_SOURCES})
source_group("Predef Files"    FILES ${DMQ_PREDEF_SOURCES})

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/state-machine
    ${CMAKE_SOURCE_DIR}/examples
    ${DMQ_INCLUDE_DIR}
)

add_executable(StateMachineApp ${SOURCES} ${DMQ_LIB_SOURCES})

# -----------------------------------------------------------------------------
# PRECOMPILED HEADERS (PCH)
# This single command works for both MSVC (Windows) and GCC/Clang (Linux).
# It compiles DelegateMQ.h once and auto-includes it into every source file
# in the target, significantly speeding up compilation.
# -----------------------------------------------------------------------------
target_precompile_headers(StateMachineApp PRIVATE 
    "${CMAKE_SOURCE_DIR}/delegate-mq/DelegateMQ.h"
)
