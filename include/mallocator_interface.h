#ifndef MALLOCATOR_INTERFACE_H
#define MALLOCATOR_INTERFACE_H

#include "mallocator.h"

/*
 * Generic memory allocator interface.
 * - Allows for an object oriented, reference counted, hierarchical allocator
 * - Provides C stdlib style interface
 */

typedef struct
{
    mallocator_t *(*create_child)(void *parent_obj, const char *name);

    void (*reference)(void *obj);

    void (*dereference)(void *obj);

    const char *(*name)(void *obj);

    void *(*malloc)(void *obj, size_t size);

    void *(*calloc)(void *obj, size_t nmemb, size_t size);

    void *(*realloc)(void *obj, void *old_ptr, size_t old_size, size_t new_size);

    void (*free)(void *obj, void *ptr, size_t size);

} mallocator_interface_t;

struct mallocator
{
    void *obj;					/* 'Subclass instance pointer' */
    const mallocator_interface_t *interface;	/* 'Subclass virtual function table' */
};

#endif // MALLOCATOR_INTERFACE_H
