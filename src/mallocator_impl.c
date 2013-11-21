#include "mallocator_impl.h"

mallocator_impl_t *mallocator_impl_create_child(mallocator_impl_t *parent, const char *name)
{
    return parent->interface->create_child(parent->obj, name);
}

void mallocator_impl_destroy(mallocator_impl_t *impl)
{
    return impl->interface->destroy(impl->obj);
}

void *mallocator_impl_malloc(mallocator_impl_t *impl, size_t size)
{
    return impl->interface->malloc(impl->obj, size);
}

void *mallocator_impl_calloc(mallocator_impl_t *impl, size_t nmemb, size_t size)
{
    return impl->interface->calloc(impl->obj, nmemb, size);
}

void *mallocator_impl_realloc(mallocator_impl_t *impl, void *ptr, size_t size, size_t new_size)
{
    return impl->interface->realloc(impl->obj, ptr, size, new_size);
}

void mallocator_impl_free(mallocator_impl_t *impl, void *ptr, size_t size)
{
    impl->interface->free(impl->obj, ptr, size);
}
