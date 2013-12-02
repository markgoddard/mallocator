include_directories(${PROJECT_SOURCE_DIR}/include)

list(APPEND MALLOCATOR_TESTS_SRC mallocator_tests.c)
list(APPEND MALLOCATOR_TESTS_SRC mallocator_test.c)
list(APPEND MALLOCATOR_TESTS_SRC mallocator_monkey_test.c)
list(APPEND MALLOCATOR_TESTS_SRC mallocator_tracer_test.c)
list(APPEND MALLOCATOR_TESTS_SRC mallocator_concurrency_test.c)

list(APPEND CMAKE_LIBRARY_PATH /usr/local/lib)
add_executable(mallocator_tests ${MALLOCATOR_TESTS_SRC})
target_link_libraries(mallocator_tests mallocator rt cgreen)
