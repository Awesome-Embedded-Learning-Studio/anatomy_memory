cmake_minimum_required(VERSION 3.25)
project(anatomy_memory VERSION 0.2.0 LANGUAGES CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# AddressSanitizer 开关：cmake -B build -DMEMORY_POOL_ASAN=ON
option(MEMORY_POOL_ASAN "Enable AddressSanitizer" OFF)
if(MEMORY_POOL_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address)
endif()

# doctest（单头测试框架）
include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.12
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(doctest)

# benchmark
add_executable(memory_pool_benchmark benchmark.cpp)
target_link_libraries(memory_pool_benchmark PRIVATE Threads::Threads)

# tests
enable_testing()
add_executable(test_basic tests/test_basic.cpp)
target_include_directories(test_basic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_basic PRIVATE doctest::doctest)
add_test(NAME test_basic COMMAND test_basic)

add_executable(test_correctness tests/test_correctness.cpp)
target_include_directories(test_correctness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_correctness PRIVATE doctest::doctest Threads::Threads)
add_test(NAME test_correctness COMMAND test_correctness)
