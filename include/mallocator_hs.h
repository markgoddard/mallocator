#ifndef MALLOCATOR_HS_H
#define MALLOCATOR_HS_H

#include "mallocator.h"

/*
 * HS (Hierarchical Statistics) mallocator implementation:
 * - Implements the mallocator interface
 * - Tree hierarchy
 * - Collects statistics on allocation
 * - Provides 'decorator' functionality, requires another mallocator to perform allocation
 */
typedef struct
{
    size_t blocks_allocated;
    size_t blocks_freed;
    size_t blocks_failed;
    size_t bytes_allocated;
    size_t bytes_freed;
    size_t bytes_failed;
} mallocator_hs_stats_t;

typedef struct mallocator_hs mallocator_hs_t;

mallocator_hs_t *mallocator_hs_create(const char *name);

mallocator_hs_t *mallocator_hs_create_custom(const char *name, mallocator_t *impl);

mallocator_t *mallocator_hs_mallocator(mallocator_hs_t *mallocator);

mallocator_hs_t *mallocator_hs_child_begin(mallocator_hs_t *mallocator);

mallocator_hs_t *mallocator_hs_child_next(mallocator_hs_t *mallocator);

mallocator_hs_t *mallocator_hs_child_lookup(mallocator_hs_t *mallocator, const char *name);

typedef void (*mallocator_hs_iter_fn)(void *arg, mallocator_hs_t *mallocator);

void mallocator_hs_iterate(mallocator_hs_t *mallocator, mallocator_hs_iter_fn fn, void *arg);

void mallocator_hs_stats(mallocator_hs_t *mallocator, mallocator_hs_stats_t *stats);

#endif // MALLOCATOR_HS_H
