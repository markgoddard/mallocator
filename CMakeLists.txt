cmake_minimum_required(VERSION 2.6)
project(mallocator)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=c99")

add_subdirectory(src)
add_subdirectory(example)
