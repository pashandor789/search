cmake_minimum_required(VERSION 3.14)

project(LSM)

set(CMAKE_CXX_STANDARD 23)

add_executable(
        lsm
        main.cpp
        lsm.cpp
)

include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
        spdlog
        URL https://github.com/gabime/spdlog/archive/refs/tags/v1.11.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(spdlog)

target_link_libraries(lsm PRIVATE spdlog::spdlog)

add_executable(
        lsm_tests
        lsm_tests.cpp
)

target_link_libraries(lsm_tests gtest gtest_main spdlog::spdlog)

enable_testing()

add_test(NAME LSMTest COMMAND lsm_tests)