#include "mallocator_hs.h"
#include "mallocator.h"
#include "mallocator_interface.h"
#include "mallocator_std.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

static void mallocator_hs_stats_init(mallocator_hs_stats_t *stats)
{
    *stats = (mallocator_hs_stats_t)
    {
	.blocks_allocated = 0,
	.blocks_freed = 0,
	.blocks_failed = 0,
	.bytes_allocated = 0,
	.bytes_freed = 0,
	.bytes_failed = 0,
    };
}

struct mallocator_hs
{
    mallocator_t mallocator;
    mallocator_t *impl;		    /* Underlying mallocator implementation */
    pthread_mutex_t lock;
    unsigned ref_count;
    const char *name;
    mallocator_hs_t *parent;
    mallocator_hs_t *children;
    mallocator_hs_t *next_child;
    mallocator_hs_stats_t stats;
};

/**************************************************************************************************/
/* mallocator_t interface */

static mallocator_t *mallocator_hs_create_child(void *parent_obj, const char *name);
static void mallocator_hs_reference(void *obj);
static void mallocator_hs_dereference(void *obj);
static const char *mallocator_hs_name(void *obj);
static void *mallocator_hs_malloc(void *obj, size_t size);
static void *mallocator_hs_calloc(void *obj, size_t nmemb, size_t size);
static void *mallocator_hs_realloc(void *obj, void *ptr, size_t size, size_t new_size);
static void mallocator_hs_free(void *obj, void *p, size_t size);

static mallocator_interface_t mallocator_hs_interface =
{
    .create_child = mallocator_hs_create_child,
    .reference = mallocator_hs_reference,
    .dereference = mallocator_hs_dereference,
    .name = mallocator_hs_name,
    .malloc = mallocator_hs_malloc,
    .calloc = mallocator_hs_calloc,
    .realloc = mallocator_hs_realloc,
    .free = mallocator_hs_free,
};

/**************************************************************************************************/
/* Private interface */

static inline mallocator_hs_t *mallocator_hs_verify(void *obj)
{
    mallocator_hs_t *mallocator = obj;
    assert(mallocator);
    assert(mallocator->ref_count > 0);
    return mallocator;
}

static void mallocator_hs_init(mallocator_hs_t *mallocator, const char *name, mallocator_t *impl)
{
    *mallocator = (mallocator_hs_t)
    {
	.mallocator =
	{
	    .obj = mallocator,
	    .interface = &mallocator_hs_interface,
	},
	.impl = impl,
	.ref_count = 1,
	.name = name,
	.parent = NULL,
	.children = NULL,
	.next_child = NULL,
    };
    assert(pthread_mutex_init(&mallocator->lock, NULL) == 0);
    mallocator_hs_stats_init(&mallocator->stats);
}

static void mallocator_hs_fini(mallocator_hs_t *mallocator)
{
    assert(pthread_mutex_destroy(&mallocator->lock) == 0);
    *mallocator = (mallocator_hs_t)
    {
	.mallocator =
	{
	    .obj = NULL,
	    .interface = NULL,
	},
	.impl = 0,
	.ref_count = 0,
	.name = NULL,
	.parent = NULL,
	.children = NULL,
	.next_child = NULL,
    };
}

static inline void mallocator_hs_lock(mallocator_hs_t *mallocator)
{
    assert(pthread_mutex_lock(&mallocator->lock) == 0);
}

static inline void mallocator_hs_unlock(mallocator_hs_t *mallocator)
{
    assert(pthread_mutex_unlock(&mallocator->lock) == 0);
}

static void mallocator_hs_child_add(mallocator_hs_t *parent, mallocator_hs_t *child)
{
    mallocator_hs_reference(parent);
    mallocator_hs_reference(child);
    mallocator_hs_lock(parent);
    child->parent = parent;
    if (parent->children)
	child->next_child = parent->children;
    else
	child->next_child = NULL;
    parent->children = child;
    mallocator_hs_unlock(parent);
}

static void mallocator_hs_child_remove(mallocator_hs_t *parent, mallocator_hs_t *child)
{
    mallocator_hs_lock(parent);
    if (parent->children == child)
    {
	parent->children = child->next_child;
    }
    else
    {
	for (mallocator_hs_t *c = parent->children; c->next_child != NULL; c = c->next_child)
	{
	    if (c->next_child == child)
	    {
		c->next_child = child->next_child;
		break;
	    }
	}
    }
    mallocator_hs_unlock(parent);
    mallocator_hs_dereference(parent);
}

static mallocator_hs_t *mallocator_hs_create_int(const char *name, mallocator_t *impl, mallocator_hs_t *parent)
{
    mallocator_hs_t *mallocator = malloc(sizeof(*mallocator));
    if (!mallocator) return NULL;

    mallocator_hs_init(mallocator, name, impl);

    if (parent)
	mallocator_hs_child_add(parent, mallocator);

    return mallocator;
}

static void mallocator_hs_destroy(mallocator_hs_t *mallocator)
{
    if (mallocator->parent)
	mallocator_hs_child_remove(mallocator->parent, mallocator);

    mallocator_dereference(mallocator->impl);
    mallocator->impl = NULL;

    mallocator_hs_fini(mallocator);
    free(mallocator);
}

static mallocator_t *mallocator_hs_create_child(void *parent_obj, const char *name)
{
    mallocator_hs_t *parent = mallocator_hs_verify(parent_obj);
    mallocator_t *child_impl = mallocator_create_child(parent->impl, name);
    if (!child_impl)
	return NULL;

    mallocator_hs_t *child = mallocator_hs_create_int(name, child_impl, parent);
    if (!child)
    {
	mallocator_dereference(child_impl);
	return NULL;
    }
    return &child->mallocator;
}

static void mallocator_hs_reference(void *obj)
{
    mallocator_hs_t *mallocator = mallocator_hs_verify(obj);

    mallocator_hs_lock(mallocator);
    mallocator->ref_count++;
    mallocator_hs_unlock(mallocator);
}

static void mallocator_hs_dereference(void *obj)
{
    mallocator_hs_t *mallocator = mallocator_hs_verify(obj);

    mallocator_hs_lock(mallocator);
    mallocator->ref_count--;
    const unsigned destroy_ref_count = mallocator->parent ? 1 : 0;
    const bool destroy = mallocator->ref_count == destroy_ref_count;
    mallocator_hs_unlock(mallocator);

    if (destroy)
	mallocator_hs_destroy(mallocator);
}

static const char *mallocator_hs_name(void *obj)
{
    mallocator_hs_t *mallocator = mallocator_hs_verify(obj);
    return mallocator->name;
}

static void *mallocator_hs_malloc(void *obj, size_t size)
{
    mallocator_hs_t *mallocator = mallocator_hs_verify(obj);

    void *ptr = mallocator_malloc(mallocator->impl, size);

    mallocator_hs_lock(mallocator);
    if (ptr)
    {
	mallocator->stats.blocks_allocated++;
	mallocator->stats.bytes_allocated += size;
    }
    else
    {
	mallocator->stats.blocks_failed++;
	mallocator->stats.bytes_failed += size;
    }
    mallocator_hs_unlock(mallocator);
    return ptr;
}

static void *mallocator_hs_calloc(void *obj, size_t nmemb, size_t size)
{
    mallocator_hs_t *mallocator = mallocator_hs_verify(obj);

    void *ptr = mallocator_calloc(mallocator->impl, nmemb, size);

    mallocator_hs_lock(mallocator);
    if (ptr)
    {
	mallocator->stats.blocks_allocated++;
	mallocator->stats.bytes_allocated += size;
    }
    else
    {
	mallocator->stats.blocks_failed++;
	mallocator->stats.bytes_failed += size;
    }
    mallocator_hs_unlock(mallocator);
    return ptr;
}

static void *mallocator_hs_realloc(void *obj, void *ptr, size_t size, size_t new_size)
{
    mallocator_hs_t *mallocator = mallocator_hs_verify(obj);

    void *new_ptr = mallocator_realloc(mallocator->impl, ptr, size, new_size);

    mallocator_hs_lock(mallocator);
    if (new_ptr)
    {
	mallocator->stats.blocks_allocated++;
	mallocator->stats.blocks_freed++;
	mallocator->stats.bytes_freed += size;
	mallocator->stats.bytes_allocated += new_size;
    }
    else
    {
	mallocator->stats.blocks_failed++;
	mallocator->stats.bytes_failed += new_size;
    }
    mallocator_hs_unlock(mallocator);
    return new_ptr;
}

static void mallocator_hs_free(void *obj, void *ptr, size_t size)
{
    mallocator_hs_t *mallocator = mallocator_hs_verify(obj);

    mallocator_free(mallocator->impl, ptr, size);

    mallocator_hs_lock(mallocator);
    mallocator->stats.blocks_freed++;
    mallocator->stats.bytes_freed += size;
    mallocator_hs_unlock(mallocator);
}

/**************************************************************************************************/
/* Public interface */

mallocator_hs_t *mallocator_hs_create(const char *name)
{
    mallocator_std_t *mallocator_std_impl = mallocator_std_create(name);
    if (!mallocator_std_impl)
	return NULL;

    mallocator_t *mallocator_std = mallocator_std_mallocator(mallocator_std_impl);
    mallocator_hs_t *mallocator = mallocator_hs_create_int(name, mallocator_std, NULL);
    if (!mallocator)
    {
	mallocator_dereference(mallocator_std);
	return NULL;
    }
    return mallocator;
}

mallocator_hs_t *mallocator_hs_create_custom(const char *name, mallocator_t *impl)
{
    mallocator_hs_t *mallocator = mallocator_hs_create_int(name, impl, NULL);
    if (!mallocator) return NULL;
    return mallocator;
}

mallocator_t *mallocator_hs_mallocator(mallocator_hs_t *mallocator)
{
    return &mallocator->mallocator;
}

void mallocator_hs_stats(mallocator_hs_t *mallocator, mallocator_hs_stats_t *stats)
{
    mallocator_hs_lock(mallocator);
    *stats = mallocator->stats;
    mallocator_hs_unlock(mallocator);
}

void mallocator_hs_iterate(mallocator_hs_t *mallocator, mallocator_hs_iter_fn fn, void *arg)
{
    mallocator_hs_lock(mallocator);
    for (mallocator_hs_t *child = mallocator->children;
	 child != NULL;
	 child = child->next_child)
    {
	fn(arg, child);
    }
    mallocator_hs_unlock(mallocator);
}
