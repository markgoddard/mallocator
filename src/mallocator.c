#include "mallocator.h"
#include "mallocator_impl.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>

static void mallocator_stats_init(mallocator_stats_t *stats)
{
    *stats = (mallocator_stats_t)
    {
	.blocks_allocated = 0,
	.blocks_freed = 0,
	.blocks_failed = 0,
	.bytes_allocated = 0,
	.bytes_freed = 0,
	.bytes_failed = 0,
    };
}

struct mallocator
{
    mallocator_impl_t *pimpl;			/* Underlying mallocator implementation (may be NULL) */
    pthread_mutex_t lock;
    unsigned ref_count;				/* Protected by lock */
    const char *name;
    mallocator_t *parent;
    mallocator_t *children;			/* Protected by lock */
    mallocator_t *next_child;			/* Protected by lock */
    mallocator_stats_t stats;			/* Protected by lock */
};

/**************************************************************************************************/
/* Private interface */

static inline void mallocator_verify(const mallocator_t *mallocator)
{
    assert(mallocator);
    assert(mallocator->ref_count > 0);
}

static void mallocator_init(mallocator_t *mallocator, const char *name, mallocator_impl_t *pimpl)
{
    *mallocator = (mallocator_t)
    {
	.pimpl = pimpl,
	.ref_count = 1,
	.name = name,
	.parent = NULL,
	.children = NULL,
	.next_child = NULL,
    };
    assert(pthread_mutex_init(&mallocator->lock, NULL) == 0);
    mallocator_stats_init(&mallocator->stats);
}

static void mallocator_fini(mallocator_t *mallocator)
{
    assert(pthread_mutex_destroy(&mallocator->lock) == 0);
    *mallocator = (mallocator_t)
    {
	.pimpl = NULL,
	.ref_count = 0,
	.name = NULL,
	.parent = NULL,
	.children = NULL,
	.next_child = NULL,
    };
}

static inline void mallocator_lock(mallocator_t *mallocator)
{
    assert(pthread_mutex_lock(&mallocator->lock) == 0);
}

static inline void mallocator_unlock(mallocator_t *mallocator)
{
    assert(pthread_mutex_unlock(&mallocator->lock) == 0);
}

static void mallocator_child_add(mallocator_t *parent, mallocator_t *child)
{
    mallocator_reference(parent);
    mallocator_reference(child);
    mallocator_lock(parent);
    child->parent = parent;
    if (parent->children)
	child->next_child = parent->children;
    else
	child->next_child = NULL;
    parent->children = child;
    mallocator_unlock(parent);
}

static void mallocator_child_remove(mallocator_t *parent, mallocator_t *child)
{
    mallocator_lock(parent);
    if (parent->children == child)
    {
	parent->children = child->next_child;
    }
    else
    {
	for (mallocator_t *c = parent->children; c->next_child != NULL; c = c->next_child)
	{
	    if (c->next_child == child)
	    {
		c->next_child = child->next_child;
		break;
	    }
	}
    }
    mallocator_unlock(parent);
    mallocator_dereference(parent);
}

static mallocator_t *mallocator_create_int(const char *name, mallocator_impl_t *pimpl, mallocator_t *parent)
{
    mallocator_t *mallocator = malloc(sizeof(*mallocator));
    if (!mallocator) return NULL;

    mallocator_init(mallocator, name, pimpl);

    if (parent)
	mallocator_child_add(parent, mallocator);

    return mallocator;
}

static void mallocator_destroy(mallocator_t *mallocator)
{
    if (mallocator->parent)
	mallocator_child_remove(mallocator->parent, mallocator);

    if (mallocator->pimpl)
    {
	mallocator_impl_destroy(mallocator->pimpl);
	mallocator->pimpl = NULL;
    }

    mallocator_fini(mallocator);
    free(mallocator);
}

/**************************************************************************************************/
/* Public interface */

mallocator_t *mallocator_create(const char *name)
{
    return mallocator_create_int(name, NULL, NULL);
}

mallocator_t *mallocator_create_child(mallocator_t *parent, const char *name)
{
    mallocator_verify(parent);

    mallocator_impl_t *child_pimpl = NULL;
    if (parent->pimpl)
    {
	child_pimpl = mallocator_impl_create_child(parent->pimpl, name);
	if (!child_pimpl)
	    return NULL;
    }

    mallocator_t *child = mallocator_create_int(name, child_pimpl, parent);
    if (!child)
    {
	if (child_pimpl)
	    mallocator_impl_destroy(child_pimpl);
	return NULL;
    }
    return child;
}

mallocator_t *mallocator_create_custom(const char *name, mallocator_impl_t *pimpl)
{
    return mallocator_create_int(name, pimpl, NULL);
}

void mallocator_reference(mallocator_t *mallocator)
{
    mallocator_verify(mallocator);

    mallocator_lock(mallocator);
    mallocator->ref_count++;
    mallocator_unlock(mallocator);
}

void mallocator_dereference(mallocator_t *mallocator)
{
    mallocator_verify(mallocator);

    mallocator_lock(mallocator);
    mallocator->ref_count--;
    const unsigned destroy_ref_count = mallocator->parent ? 1 : 0;
    const bool destroy = mallocator->ref_count == destroy_ref_count;
    mallocator_unlock(mallocator);

    if (destroy)
	mallocator_destroy(mallocator);
}

const char *mallocator_name(mallocator_t *mallocator)
{
    mallocator_verify(mallocator);
    return mallocator->name;
}

const char *mallocator_full_name(mallocator_t *mallocator, char *buf, size_t buf_len)
{
    mallocator_verify(mallocator);
    if (mallocator->parent)
    {
	mallocator_full_name(mallocator->parent, buf, buf_len);
	strcat(buf, ".");
	strcat(buf, mallocator->name);
    }
    else
    {
	strcpy(buf, mallocator->name);
    }
    return buf;
}

mallocator_t *mallocator_child_begin(mallocator_t *mallocator)
{
    mallocator_verify(mallocator);

    mallocator_lock(mallocator);
    mallocator_t *child = mallocator->children;
    if (child) mallocator_reference(child);
    mallocator_unlock(mallocator);
    return child;
}

mallocator_t *mallocator_child_next(mallocator_t *mallocator)
{
    mallocator_verify(mallocator);

    mallocator_lock(mallocator);
    mallocator_t *next = mallocator->next_child;
    if (next) mallocator_reference(next);
    mallocator_unlock(mallocator);
    mallocator_dereference(mallocator);
    return next;
}

mallocator_t *mallocator_child_lookup(mallocator_t *mallocator, const char *name)
{
    mallocator_verify(mallocator);

    for (mallocator_t *child = mallocator_child_begin(mallocator);
	 child != NULL;
	 child = mallocator_child_next(child))
    {
	const int res = strcmp(child->name, name);
	if (res == 0)
	    return child;
    }
    return NULL;
}

void mallocator_iterate(mallocator_t *mallocator, mallocator_iter_fn fn, void *arg)
{
    mallocator_verify(mallocator);

    for (mallocator_t *child = mallocator_child_begin(mallocator);
	 child != NULL;
	 child = mallocator_child_next(child))
    {
	fn(arg, child);
    }
}

void mallocator_stats(mallocator_t *mallocator, mallocator_stats_t *stats)
{
    mallocator_verify(mallocator);

    mallocator_lock(mallocator);
    *stats = mallocator->stats;
    mallocator_unlock(mallocator);
}

void *mallocator_malloc(mallocator_t *mallocator, size_t size)
{
    mallocator_verify(mallocator);

    void *ptr;
    if (mallocator->pimpl)
    {
	ptr = mallocator_impl_malloc(mallocator->pimpl, size);
    }
    else
    {
	ptr = malloc(size);
    }

    mallocator_lock(mallocator);
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
    mallocator_unlock(mallocator);
    return ptr;
}

void *mallocator_calloc(mallocator_t *mallocator, size_t nmemb, size_t size)
{
    mallocator_verify(mallocator);

    void *ptr;
    if (mallocator->pimpl)
    {
	ptr = mallocator_impl_calloc(mallocator->pimpl, nmemb, size);
    }
    else
    {
	ptr = calloc(nmemb, size);
    }

    mallocator_lock(mallocator);
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
    mallocator_unlock(mallocator);
    return ptr;
}

void *mallocator_realloc(mallocator_t *mallocator, void *ptr, size_t size, size_t new_size)
{
    mallocator_verify(mallocator);

    void *new_ptr;
    if (mallocator->pimpl)
    {
	new_ptr = mallocator_impl_realloc(mallocator->pimpl, ptr, size, new_size);
    }
    else
    {
	new_ptr = realloc(ptr, new_size);
    }

    mallocator_lock(mallocator);
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
    mallocator_unlock(mallocator);
    return new_ptr;
}

void mallocator_free(mallocator_t *mallocator, void *ptr, size_t size)
{
    mallocator_verify(mallocator);

    if (mallocator->pimpl)
    {
	mallocator_impl_free(mallocator->pimpl, ptr, size);
    }
    else
    {
	free(ptr);
    }

    mallocator_lock(mallocator);
    mallocator->stats.blocks_freed++;
    mallocator->stats.bytes_freed += size;
    mallocator_unlock(mallocator);
}
