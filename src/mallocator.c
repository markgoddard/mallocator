#include "mallocator.h"
#include "mallocator_interface.h"

mallocator_t *mallocator_create_child(mallocator_t *parent, const char *name)
{
    return parent->interface->create_child(parent->obj, name);
}

void mallocator_reference(mallocator_t *mallocator)
{
    return mallocator->interface->reference(mallocator->obj);
}

void mallocator_dereference(mallocator_t *mallocator)
{
    mallocator->interface->dereference(mallocator->obj);
}

const char *mallocator_name(mallocator_t *mallocator)
{
    return mallocator->interface->name(mallocator->obj);
}

void *mallocator_malloc(mallocator_t *mallocator, size_t size)
{
    return mallocator->interface->malloc(mallocator->obj, size);
}

void *mallocator_calloc(mallocator_t *mallocator, size_t nmemb, size_t size)
{
    return mallocator->interface->calloc(mallocator->obj, nmemb, size);
}

void *mallocator_realloc(mallocator_t *mallocator, void *ptr, size_t size, size_t new_size)
{
    return mallocator->interface->realloc(mallocator->obj, ptr, size, new_size);
}

void mallocator_free(mallocator_t *mallocator, void *ptr, size_t size)
{
    mallocator->interface->free(mallocator->obj, ptr, size);
}
