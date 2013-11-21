#ifndef MALLOCATOR_H
#define MALLOCATOR_H

/*
 * Memory allocator interface:
 * - Provides C stdlib style interface
 * - Allows for an object oriented, reference counted, hierarchical allocator
 * - Collects statistics on allocation
 */

#include <stdlib.h>

/* Opaque allocator object */
typedef struct mallocator mallocator_t;

/* Allocation statistics */
typedef struct
{
    size_t blocks_allocated;
    size_t blocks_freed;
    size_t blocks_failed;
    size_t bytes_allocated;
    size_t bytes_freed;
    size_t bytes_failed;
} mallocator_stats_t;

mallocator_t *mallocator_create(const char *name);

mallocator_t *mallocator_create_child(mallocator_t *parent, const char *name);

void mallocator_reference(mallocator_t *mallocator);

void mallocator_dereference(mallocator_t *mallocator);

const char *mallocator_name(mallocator_t *mallocator);

const char *mallocator_full_name(mallocator_t *mallocator, char *buf, size_t buf_len);

mallocator_t *mallocator_child_begin(mallocator_t *mallocator);

mallocator_t *mallocator_child_next(mallocator_t *mallocator);

mallocator_t *mallocator_child_lookup(mallocator_t *mallocator, const char *name);

typedef void (*mallocator_iter_fn)(void *arg, mallocator_t *mallocator);

void mallocator_iterate(mallocator_t *mallocator, mallocator_iter_fn fn, void *arg);

void mallocator_stats(mallocator_t *mallocator, mallocator_stats_t *stats);

/* Allocation */

void *mallocator_malloc(mallocator_t *mallocator, size_t size);

void *mallocator_calloc(mallocator_t *mallocator, size_t nmem, size_t size);

void *mallocator_realloc(mallocator_t *mallocator, void *ptr, size_t size, size_t new_size);

void mallocator_free(mallocator_t *mallocator, void *ptr, size_t size);

#endif // MALLOCATOR_H
