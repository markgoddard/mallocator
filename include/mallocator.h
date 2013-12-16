#ifndef MALLOCATOR_H
#define MALLOCATOR_H

/*
 * Memory allocator interface:
 * - Provides C stdlib style interface
 * - Allows for an object oriented, reference counted, hierarchical allocator
 * - Collects statistics on allocation
 */

#include <stdlib.h>

/**
 * Opaque memory allocator object.
 */
typedef struct mallocator mallocator_t;

/**
 * Allocation statistics.
 */
typedef struct
{
    size_t blocks_allocated;
    size_t blocks_freed;
    size_t blocks_failed;
    size_t bytes_allocated;
    size_t bytes_freed;
    size_t bytes_failed;
} mallocator_stats_t;

/**
 * Create a root mallocator.
 */
mallocator_t *mallocator_create(const char *name);

/**
 * Create a child mallocator to parent. This will fail if a child exists with the same name.
 */
mallocator_t *mallocator_create_child(mallocator_t *parent, const char *name);

/**
 * Add a reference to mallocator.
 */
void mallocator_reference(mallocator_t *mallocator);

/**
 * Release a reference to mallocator.
 */
void mallocator_dereference(mallocator_t *mallocator);

/**
 * Return the name of mallocator.
 */
const char *mallocator_name(mallocator_t *mallocator);

/**
 * Return the full hierarchical name of mallocator. Generation names are separated by a period.
 */
const char *mallocator_full_name(mallocator_t *mallocator, char *buf, size_t buf_len);

/**
 * Return the parent of mallocator if this is not a root mallocator. The parent is referenced on
 * return.
 */
mallocator_t *mallocator_parent(mallocator_t *mallocator);

/**
 * Return the first child of mallocator if this is a parent. The child is referenced on return.
 */
mallocator_t *mallocator_child_begin(mallocator_t *mallocator);

/**
 * Return the next sibling of mallocator if this is not the last child. The sibling is referenced
 * on return.
 */
mallocator_t *mallocator_child_next(mallocator_t *mallocator);

/**
 * Lookup a specific child of mallocator by name.
 */
mallocator_t *mallocator_child_lookup(mallocator_t *mallocator, const char *name);

typedef void (*mallocator_iter_fn)(void *arg, mallocator_t *mallocator);

/**
 * Iterate over the children of mallocator, calling the provided callback function once for each.
 */
void mallocator_iterate(mallocator_t *mallocator, mallocator_iter_fn fn, void *arg);

/**
 * Return usage statistics for mallocator.
 */
void mallocator_stats(mallocator_t *mallocator, mallocator_stats_t *stats);

/* Allocation */

/**
 * See malloc.
 */
void *mallocator_malloc(mallocator_t *mallocator, size_t size);

/**
 * See calloc.
 */
void *mallocator_calloc(mallocator_t *mallocator, size_t nmem, size_t size);

/**
 * See realloc.
 * \note Unlike realloc, the old size should be passed in for statistic collection.
 */
void *mallocator_realloc(mallocator_t *mallocator, void *ptr, size_t size, size_t new_size);

/**
 * See free.
 * \note Unlike free, the allocated size should be passed in for statistic collection.
 */
void mallocator_free(mallocator_t *mallocator, void *ptr, size_t size);

/**
 * Leak reporter function.
 * The function will be called during the destruction of a mallocator which has allocated more
 * memory than it has freed.
 * \note The function will be called from a sensitive context, so should not be blocked e.g. by
 * attempting to acquire a lock.
 */
typedef void (*mallocator_leak_reporter_fn)(void *arg, const char *name, size_t blocks_leaked, size_t bytes_leaked);

/**
 * Set a leak reporter function for mallocator and all children.
 * \note mallocator should be a root mallocator.
 */
void mallocator_set_leak_reporter(mallocator_t *mallocator, mallocator_leak_reporter_fn fn, void *arg);

/**
 * Clear a leak reporter function for mallocator and all children.
 * \note mallocator should be a root mallocator.
 */
void mallocator_clear_leak_reporter(mallocator_t *mallocator);

#endif // MALLOCATOR_H
