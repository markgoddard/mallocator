include_directories(${PROJECT_SOURCE_DIR}/include)

list(APPEND MALLOCATOR_SRC mallocator.c)
list(APPEND MALLOCATOR_SRC mallocator_monkey.c)
list(APPEND MALLOCATOR_SRC mallocator_tracer.c)
list(APPEND MALLOCATOR_SRC default_mallocator.c)

add_library(mallocator ${MALLOCATOR_SRC})
