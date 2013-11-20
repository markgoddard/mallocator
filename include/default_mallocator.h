#ifndef DEFAULT_MALLOCATOR_H
#define DEFAULT_MALLOCATOR_H

/*
 * Default module mallocator
 * Provides access to a global mallocator singleton object.
 */

#include "mallocator.h"

/* Public API */

void default_mallocator_init(mallocator_t *mallocator);

void default_mallocator_fini(void);

mallocator_t *default_mallocator(void);

mallocator_t *default_mallocator_create_child(const char *name);

/* Allocation helper functions */

static inline void *default_mallocator_malloc(size_t size)
{
    return mallocator_malloc(default_mallocator(), size);
}

static inline void *default_mallocator_calloc(size_t nmem, size_t size)
{
    return mallocator_calloc(default_mallocator(), nmem, size);
}

static inline void *default_mallocator_realloc(void *ptr, size_t size, size_t new_size)
{
    return mallocator_realloc(default_mallocator(), ptr, size, new_size);
}

static inline void default_mallocator_free(void *ptr, size_t size)
{
    mallocator_free(default_mallocator(), ptr, size);
}

#endif // DEFAULT_MALLOCATOR_H
