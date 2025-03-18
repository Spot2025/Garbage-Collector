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
        lib/user_funcs_impl.cpp)

add_executable(Tests
        tests/test.cpp
        tests/benchmarks.cpp)

add_executable(Benchmarks
        tests/benchmarks.cpp)

target_link_libraries(Tests Lib gtest gtest_main)
target_link_libraries(Benchmarks Lib gtest gtest_main)