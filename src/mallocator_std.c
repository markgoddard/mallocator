#include "mallocator.h"
#include "mallocator_interface.h"
#include "mallocator_std.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

struct mallocator_std
{
    mallocator_t mallocator;
    pthread_mutex_t lock;	    /* Protects ref_count */
    unsigned ref_count;
    const char *name;
};

/**************************************************************************************************/
/* mallocator_t interface */

static mallocator_t *mallocator_std_create_child(void *parent_obj, const char *name);
static void mallocator_std_reference(void *obj);
static void mallocator_std_dereference(void *obj);
static const char *mallocator_std_name(void *obj);
static void *mallocator_std_malloc(void *obj, size_t size);
static void *mallocator_std_calloc(void *obj, size_t nmemb, size_t size);
static void *mallocator_std_realloc(void *obj, void *ptr, size_t size, size_t new_size);
static void mallocator_std_free(void *obj, void *p, size_t size);

static mallocator_interface_t mallocator_std_interface =
{
    .create_child = mallocator_std_create_child,
    .reference = mallocator_std_reference,
    .dereference = mallocator_std_dereference,
    .name = mallocator_std_name,
    .malloc = mallocator_std_malloc,
    .calloc = mallocator_std_calloc,
    .realloc = mallocator_std_realloc,
    .free = mallocator_std_free,
};

/**************************************************************************************************/

static inline mallocator_std_t *mallocator_std_verify(void *obj)
{
    mallocator_std_t *mallocator = obj;
    assert(mallocator);
    assert(mallocator->ref_count > 0);
    return mallocator;
}

static void mallocator_std_init(mallocator_std_t *mallocator, const char *name)
{
    *mallocator = (mallocator_std_t)
    {
	.mallocator =
	{
	    .obj = mallocator,
	    .interface = &mallocator_std_interface,
	},
	.ref_count = 1,
	.name = name,
    };
    assert(pthread_mutex_init(&mallocator->lock, NULL) == 0);
}

static void mallocator_std_fini(mallocator_std_t *mallocator)
{
    assert(pthread_mutex_destroy(&mallocator->lock) == 0);
    *mallocator = (mallocator_std_t)
    {
	.mallocator =
	{
	    .obj = NULL,
	    .interface = NULL,
	},
	.ref_count = 0,
	.name = NULL,
    };
}

static inline void mallocator_std_lock(mallocator_std_t *mallocator)
{
    assert(pthread_mutex_lock(&mallocator->lock) == 0);
}

static inline void mallocator_std_unlock(mallocator_std_t *mallocator)
{
    assert(pthread_mutex_unlock(&mallocator->lock) == 0);
}

static mallocator_std_t *mallocator_std_create_int(const char *name)
{
    mallocator_std_t *mallocator = malloc(sizeof(*mallocator));
    if (!mallocator) return NULL;

    mallocator_std_init(mallocator, name);
    return mallocator;
}

static void mallocator_std_destroy(mallocator_std_t *mallocator)
{
    mallocator_std_fini(mallocator);
    free(mallocator);
}

static mallocator_t *mallocator_std_create_child(void *parent_obj, const char *name)
{
    mallocator_std_t *parent = mallocator_std_verify(parent_obj);
    mallocator_std_reference(parent);
    return &parent->mallocator;
}

static void mallocator_std_reference(void *obj)
{
    mallocator_std_t *mallocator = mallocator_std_verify(obj);

    mallocator_std_lock(mallocator);
    mallocator->ref_count++;
    mallocator_std_unlock(mallocator);
}

static void mallocator_std_dereference(void *obj)
{
    mallocator_std_t *mallocator = mallocator_std_verify(obj);

    mallocator_std_lock(mallocator);
    mallocator->ref_count--;
    const bool destroy = mallocator->ref_count == 0;
    mallocator_std_unlock(mallocator);

    if (destroy)
	mallocator_std_destroy(mallocator);
}

static const char *mallocator_std_name(void *obj)
{
    mallocator_std_t *mallocator = mallocator_std_verify(obj);
    return mallocator->name;
}

static void *mallocator_std_malloc(void *obj, size_t size)
{
    mallocator_std_t *mallocator = mallocator_std_verify(obj);
    return malloc(size);
}

static void *mallocator_std_calloc(void *obj, size_t nmemb, size_t size)
{
    mallocator_std_t *mallocator = mallocator_std_verify(obj);
    return calloc(nmemb, size);
}

static void *mallocator_std_realloc(void *obj, void *ptr, size_t size, size_t new_size)
{
    mallocator_std_t *mallocator = mallocator_std_verify(obj);
    return realloc(ptr, new_size);
}

static void mallocator_std_free(void *obj, void *ptr, size_t size)
{
    mallocator_std_t *mallocator = mallocator_std_verify(obj);
    free(ptr);
}

/**************************************************************************************************/
/* Public interface */

mallocator_std_t *mallocator_std_create(const char *name)
{
    mallocator_std_t *mallocator = mallocator_std_create_int(name);
    if (!mallocator) return NULL;
    return mallocator;
}

mallocator_t *mallocator_std_mallocator(mallocator_std_t *mallocator)
{
    return &mallocator->mallocator;
}
