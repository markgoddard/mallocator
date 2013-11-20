#include "default_mallocator.h"
#include <assert.h>

/* Singleton mallocator instance */
static mallocator_t *default_mallocator_instance;

void default_mallocator_init(mallocator_t *mallocator)
{
    assert(!default_mallocator_instance);
    assert(mallocator);
    mallocator_reference(mallocator);
    default_mallocator_instance = mallocator;
}

void default_mallocator_fini(void)
{
    assert(default_mallocator_instance);
    mallocator_dereference(default_mallocator_instance);
    default_mallocator_instance = NULL;
}

mallocator_t *default_mallocator(void)
{
    return default_mallocator_instance;
}

mallocator_t *default_mallocator_create_child(const char *name)
{
    return mallocator_create_child(default_mallocator(), name);
}
