#ifndef MALLOCATOR_H
#define MALLOCATOR_H

#include <stdlib.h>

/*
 * Generic memory allocator interface.
 * - Allows for an object oriented, reference counted, hierarchical allocator
 * - Provides C stdlib style interface
 */
typedef struct mallocator mallocator_t;

mallocator_t *mallocator_create_child(mallocator_t *parent, const char *name);

void mallocator_reference(mallocator_t *mallocator);

void mallocator_dereference(mallocator_t *mallocator);

const char *mallocator_name(mallocator_t *mallocator);

/* Allocation */

void *mallocator_malloc(mallocator_t *mallocator, size_t size);

void *mallocator_calloc(mallocator_t *mallocator, size_t nmem, size_t size);

void *mallocator_realloc(mallocator_t *mallocator, void *ptr, size_t size, size_t new_size);

void mallocator_free(mallocator_t *mallocator, void *ptr, size_t size);

#endif // MALLOCATOR_H
