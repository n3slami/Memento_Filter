cmake_minimum_required(VERSION 3.16.3)
project(Memento
        VERSION 1.0
        HOMEPAGE_URL https://github.com/marcocosta97/grafite)

option(BUILD_TESTS "Build the tests" ON)
option(BUILD_EXAMPLES "Build the examples" ON)
option(BUILD_BENCHMARKS "Build the benchmark targets" ON)
option(USE_BOOST "Use the Boost library" ON)
option(USE_MULTI_THREADED "Use multi-threaded version of the library" OFF)

set(CMAKE_CXX_STANDARD 17)
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
else()
    set(USE_MULTI_THREADED OFF)
endif ()

add_library(mementolib STATIC ./src/memento.c ./src/hashutil.c)
target_include_directories(mementolib PUBLIC ./include)
target_compile_options(mementolib PUBLIC -Ofast -msse4.2 -D__SSE4_2_)

if (BUILD_TESTS)
    message(STATUS "Building tests")
    add_subdirectory(tests)
endif ()

if (BUILD_EXAMPLES)
    message(STATUS "Building examples")
    add_subdirectory(examples)
endif ()

if (BUILD_BENCHMARKS)
    if (NOT EXISTS ../bench/include/grafite/README.md)
        message(STATUS "Cloning baselines")
        execute_process(COMMAND bash -c "cd ../bench/scripts && bash setup_includes.sh")
    endif()
    if (NOT EXISTS ../bench/wiredtiger/README.md)
        message(STATUS "Building WiredTiger")
        execute_process(COMMAND bash -c "git submodule update --init ../bench/wiredtiger")
        execute_process(COMMAND bash -c "cd ../bench/scripts && bash build_wiredtiger.sh")
    endif()

    message(STATUS "Building benchmarks")
    add_subdirectory(bench)
endif ()

