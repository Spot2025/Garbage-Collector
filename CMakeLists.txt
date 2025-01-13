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

add_executable(BaseTest
tests/test.cpp
        include/gc.h)

target_link_libraries(BaseTest gtest gtest_main)