cmake_minimum_required(VERSION 3.14)

project(InvertedIndex)

set(CMAKE_CXX_STANDARD 23)

add_executable(
        inverted_index
        main.cpp
        inverted_index.cpp
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

target_link_libraries(inverted_index PRIVATE spdlog::spdlog)

add_executable(
        inverted_index_tests
        inverted_index_tests.cpp
)

target_link_libraries(inverted_index_tests gtest gtest_main spdlog::spdlog)

add_custom_target(
        copy_static ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/static $<TARGET_FILE_DIR:inverted_index_tests>/static
)

enable_testing()

add_test(NAME InvertedIndexTest COMMAND inverted_index_tests)
