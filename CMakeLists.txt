cmake_minimum_required(VERSION 2.6)
project(mallocator)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(test)

enable_testing()
add_test(NAME mallocator_tests COMMAND bin/mallocator_tests)
