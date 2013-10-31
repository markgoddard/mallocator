include_directories(${PROJECT_SOURCE_DIR}/include)

list(APPEND MALLOCATOR_SRC mallocator.c)
list(APPEND MALLOCATOR_SRC mallocator_std.c)
list(APPEND MALLOCATOR_SRC mallocator_hs.c)
list(APPEND MALLOCATOR_SRC mallocator_monkey.c)

add_library(mallocator ${MALLOCATOR_SRC})
