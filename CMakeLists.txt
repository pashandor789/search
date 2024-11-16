cmake_minimum_required(VERSION 3.24)

project(search)

## Set default build type to Debug if not specified
#if(NOT CMAKE_BUILD_TYPE)
#    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
#endif()

# Enable ASan by default
# This is only applicable for GCC and Clang
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")

add_subdirectory(inverted_index)
add_subdirectory(lsm)