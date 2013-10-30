include_directories(${PROJECT_SOURCE_DIR}/include)

list(APPEND MALLOCATOR_EXAMPLE_SRC mallocator_example.c)

add_executable(mallocator_example ${MALLOCATOR_EXAMPLE_SRC})
target_link_libraries(mallocator_example mallocator)
