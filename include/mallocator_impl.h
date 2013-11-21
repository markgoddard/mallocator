#ifndef MALLOCATOR_IMPL_H
#define MALLOCATOR_IMPL_H

/*
 * Generic memory allocator interface.
 * - Allows for an object oriented allocator
 * - Provides C stdlib style interface
 */

#include "mallocator.h"

typedef struct mallocator_impl mallocator_impl_t;

typedef struct
{
    mallocator_impl_t *(*create_child)(void *parent_obj, const char *name);

    void (*destroy)(void *obj);

    void *(*malloc)(void *obj, size_t size);

    void *(*calloc)(void *obj, size_t nmemb, size_t size);

    void *(*realloc)(void *obj, void *old_ptr, size_t old_size, size_t new_size);

    void (*free)(void *obj, void *ptr, size_t size);

} mallocator_interface_t;

struct mallocator_impl
{
    void *obj;					/* 'Subclass instance pointer' */
    const mallocator_interface_t *interface;	/* 'Subclass virtual function table' */
};

/* Helper functions */

mallocator_impl_t *mallocator_impl_create_child(mallocator_impl_t *parent, const char *name);

void mallocator_impl_destroy(mallocator_impl_t *impl);

void *mallocator_impl_malloc(mallocator_impl_t *impl, size_t size);

void *mallocator_impl_calloc(mallocator_impl_t *impl, size_t nmemb, size_t size);

void *mallocator_impl_realloc(mallocator_impl_t *impl, void *ptr, size_t size, size_t new_size);

void mallocator_impl_free(mallocator_impl_t *impl, void *ptr, size_t size);


/* Create a mallocator with a custom allocator implementation */
mallocator_t *mallocator_create_custom(const char *name, mallocator_impl_t *impl);

#endif // MALLOCATOR_IMPL_H
