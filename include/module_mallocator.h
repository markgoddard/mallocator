#ifndef MODULE_MALLOCATOR_H
#define MODULE_MALLOCATOR_H

/*
 * Default module mallocator
 * Provides access to a file local mallocator singleton object.
 */

#include "mallocator.h"
#include <assert.h>

/* Private - do not use directly */
static mallocator_t *module_mallocator_instance;

/* Public API */

static inline void module_mallocator_init(mallocator_t *mallocator)
{
    assert(!module_mallocator_instance);
    assert(mallocator);
    mallocator_reference(mallocator);
    module_mallocator_instance = mallocator;
}

static inline void module_mallocator_fini(void)
{
    assert(module_mallocator_instance);
    mallocator_dereference(module_mallocator_instance);
    module_mallocator_instance = NULL;
}

static inline mallocator_t *module_mallocator(void)
{
    return module_mallocator_instance;
}

static inline mallocator_t *module_mallocator_create_child(const char *name)
{
    return mallocator_create_child(module_mallocator(), name);
}

/* Allocation helper functions */

static inline void *module_mallocator_malloc(size_t size)
{
    return mallocator_malloc(module_mallocator(), size);
}

static inline void *module_mallocator_calloc(size_t nmem, size_t size)
{
    return mallocator_calloc(module_mallocator(), nmem, size);
}

static inline void *module_mallocator_realloc(void *ptr, size_t size, size_t new_size)
{
    return mallocator_realloc(module_mallocator(), ptr, size, new_size);
}

static inline void module_mallocator_free(void *ptr, size_t size)
{
    mallocator_free(module_mallocator(), ptr, size);
}

#endif // MODULE_MALLOCATOR_H
