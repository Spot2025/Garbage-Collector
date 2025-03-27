cmake_minimum_required(VERSION 3.28)
project(GarbageCollector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

include_directories(include
                    lib)

enable_testing()  # Включаем поддержку тестирования

# Загрузка Google Test
include(FetchContent)
FetchContent_Declare(
        gtest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        release-1.11.0
)

FetchContent_MakeAvailable(gtest)

add_library(Lib
        lib/gc_impl.cpp
        lib/gc.cpp)

add_executable(Tests
        tests/test.cpp
        tests/benchmarks.cpp)

add_executable(Benchmarks
        tests/benchmarks.cpp
        tests/biggerBench.cpp
        tests/realApplicationBench.cpp)

add_executable(RealApplicationBenchmarks
        tests/realApplicationBench.cpp)

target_link_libraries(Tests Lib gtest gtest_main)

find_package(benchmark REQUIRED)

target_link_libraries(Benchmarks Lib gtest gtest_main benchmark::benchmark)
target_link_libraries(RealApplicationBenchmarks Lib gtest gtest_main benchmark::benchmark)