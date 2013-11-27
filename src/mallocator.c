#include "mallocator.h"
#include "mallocator_impl.h"

#include "atomic.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>

/* Define for lock free statistics accumulation */
#define MALLOCATOR_STATS_ATOMIC

typedef struct
{
    pthread_mutex_t lock;			/* Lock for tree synchronisation */
    mallocator_t *root;				/* Root mallocator object */
} mallocator_tree_t;

#ifdef MALLOCATOR_STATS_ATOMIC
typedef mallocator_stats_t mallocator_stats_coll_t;
#else
typedef struct
{
    pthread_mutex_t lock;			/* Lock for non-atomic stats synchronisation */
    mallocator_stats_t stats;			/* Protected by stats_lock if not atomic */
} mallocator_stats_coll_t;
#endif

struct mallocator
{
    mallocator_tree_t *tree;			/* tree containing this mallocator */
    mallocator_impl_t *pimpl;			/* Underlying mallocator implementation (may be NULL) */
    unsigned ref_count;				/* Protected by tree->lock */
    const char *name;
    mallocator_t *parent;
    mallocator_t *children;			/* Protected by tree->lock */
    mallocator_t *next_child;			/* Protected by tree->lock */
    mallocator_stats_coll_t stats;		/* Statistics collection */
};

/**************************************************************************************************/
/* Stats utilities */

static inline void mallocator_stats_init(mallocator_stats_t *stats)
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

#ifdef MALLOCATOR_STATS_ATOMIC

static void mallocator_stats_coll_init(mallocator_stats_coll_t *stats)
{
    mallocator_stats_init(stats);
}

static void mallocator_stats_coll_fini(mallocator_stats_coll_t *stats)
{
}

static inline void mallocator_stats_allocated(mallocator_t *mallocator, size_t size)
{
    atomic_fetch_add(&mallocator->stats.blocks_allocated, 1);
    atomic_fetch_add(&mallocator->stats.bytes_allocated, size);
}

static inline void mallocator_stats_freed(mallocator_t *mallocator, size_t size)
{
    atomic_fetch_add(&mallocator->stats.blocks_freed, 1);
    atomic_fetch_add(&mallocator->stats.bytes_freed, size);
}

static inline void mallocator_stats_failed(mallocator_t *mallocator, size_t size)
{
    atomic_fetch_add(&mallocator->stats.blocks_failed, 1);
    atomic_fetch_add(&mallocator->stats.bytes_failed, size);
}

static inline void mallocator_stats_get(mallocator_t *mallocator, mallocator_stats_t *stats)
{
    stats->blocks_allocated = atomic_load(&mallocator->stats.blocks_allocated);
    stats->bytes_allocated = atomic_load(&mallocator->stats.bytes_allocated);
    stats->blocks_freed = atomic_load(&mallocator->stats.blocks_freed);
    stats->bytes_freed = atomic_load(&mallocator->stats.bytes_freed);
    stats->blocks_failed = atomic_load(&mallocator->stats.blocks_failed);
    stats->bytes_failed = atomic_load(&mallocator->stats.bytes_failed);
}

#else

static void mallocator_stats_coll_init(mallocator_stats_coll_t *stats)
{
    assert(pthread_mutex_create(&stats->lock, NULL) == 0);
    mallocator_stats_init(&stats->stats);
}

static void mallocator_stats_coll_fini(mallocator_stats_coll_t *stats)
{
    assert(pthread_mutex_destroy(&stats->lock) == 0);
}

static inline void mallocator_stats_lock(mallocator_t *mallocator)
{
    assert(pthread_mutex_lock(&mallocator->stats.lock) == 0);
}

static inline void mallocator_stats_unlock(mallocator_t *mallocator)
{
    assert(pthread_mutex_unlock(&mallocator->stats.lock) == 0);
}

static inline void mallocator_stats_allocated(mallocator_t *mallocator, size_t size)
{
    mallocator_stats_lock(mallocator);
    mallocator->stats.blocks_allocated += 1;
    mallocator->stats.bytes_allocated += size;
    mallocator_stats_unlock(mallocator);
}

static inline void mallocator_stats_freed(mallocator_t *mallocator, size_t size)
{
    mallocator_stats_lock(mallocator);
    mallocator->stats.blocks_freed += 1;
    mallocator->stats.bytes_freed += size;
    mallocator_stats_unlock(mallocator);
}

static inline void mallocator_stats_failed(mallocator_t *mallocator, size_t size)
{
    mallocator_stats_lock(mallocator);
    mallocator->stats.blocks_failed += 1;
    mallocator->stats.bytes_failed += size;
    mallocator_stats_unlock(mallocator);
}

static inline void mallocator_stats_get(mallocator_t *mallocator, mallocator_stats_t *stats)
{
    mallocator_stats_lock(mallocator);
    *stats = mallocator->stats;
    mallocator_stats_unlock(mallocator);
}

#endif

/**************************************************************************************************/
/* mallocator tree hierarchy */

static mallocator_tree_t *mallocator_tree_create(mallocator_t *root)
{
    mallocator_tree_t *tree = malloc(sizeof(*tree));
    if (!tree) return NULL;
    assert(pthread_mutex_init(&tree->lock, NULL) == 0);
    tree->root = root;
    return tree;
}

static void mallocator_tree_destroy(mallocator_tree_t *tree)
{
    assert(pthread_mutex_destroy(&tree->lock) == 0);
    tree->root = NULL;
    free(tree);
}

static inline void mallocator_tree_lock(mallocator_tree_t *tree)
{
    assert(pthread_mutex_lock(&tree->lock) == 0);
}

static inline void mallocator_tree_unlock(mallocator_tree_t *tree)
{
    assert(pthread_mutex_unlock(&tree->lock) == 0);
}

/**************************************************************************************************/
/* Private interface */

static inline void mallocator_verify(const mallocator_t *mallocator)
{
    assert(mallocator);
    assert(mallocator->ref_count > 0);
}

static void mallocator_init(mallocator_t *mallocator, mallocator_tree_t *tree, const char *name, mallocator_impl_t *pimpl)
{
    *mallocator = (mallocator_t)
    {
	.tree = tree,
	.pimpl = pimpl,
	.ref_count = 1,
	.name = name,
	.parent = NULL,
	.children = NULL,
	.next_child = NULL,
    };
    mallocator_stats_coll_init(&mallocator->stats);
}

static void mallocator_fini(mallocator_t *mallocator)
{
    mallocator_stats_coll_fini(&mallocator->stats);
    *mallocator = (mallocator_t)
    {
	.tree = NULL,
	.pimpl = NULL,
	.ref_count = 0,
	.name = NULL,
	.parent = NULL,
	.children = NULL,
	.next_child = NULL,
    };
}

/* Return false if the child name exists */
static bool mallocator_child_add(mallocator_t *parent, mallocator_t *child)
{
    if (!parent->children || strcmp(child->name, parent->children->name) < 0)
    {
	/* Front of list */
	child->next_child = parent->children;
	parent->children = child;
    }
    else
    {
	mallocator_t *curr = parent->children;
	while (curr->next_child && strcmp(child->name, curr->next_child->name) > 0)
	    curr = curr->next_child;

	/* Check for duplicate name */
	if (strcmp(child->name, curr->name) == 0)
	    return false;

	/* Add after curr */
	child->next_child = curr->next_child;
	curr->next_child = child;
    }
    child->parent = parent;
    return true;
}

static void mallocator_child_remove(mallocator_t *parent, mallocator_t *child)
{
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
}

static mallocator_t *mallocator_create_int(const char *name, mallocator_impl_t *pimpl, mallocator_t *parent)
{
    mallocator_t *mallocator = malloc(sizeof(*mallocator));
    if (!mallocator) return NULL;

    mallocator_tree_t *tree;
    if (!parent)
    {
	tree = mallocator_tree_create(mallocator);
	if (!tree)
	{
	    free(mallocator);
	    return NULL;
	}
    }
    else
    {
	tree = parent->tree;
    }

    bool valid = true;
    mallocator_init(mallocator, tree, name, pimpl);
    mallocator_tree_lock(tree);
    if (parent)	valid = mallocator_child_add(parent, mallocator);
    mallocator_tree_unlock(tree);

    /* Check for duplicate name */
    if (!valid)
    {
	mallocator_fini(mallocator);
	free(mallocator);
	if (!parent) mallocator_tree_destroy(tree);
	return NULL;
    }
    return mallocator;
}

static bool mallocator_should_destroy(mallocator_t *mallocator)
{
    return mallocator->ref_count == 0 && !mallocator->children;
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

static mallocator_tree_t *mallocator_destroy_ancestors(mallocator_t *mallocator)
{
    mallocator_tree_t *tree = mallocator->tree;
    mallocator_t *parent = mallocator->parent;
    mallocator_destroy(mallocator);

    /* Root node - return the tree for destruction */
    if (!parent) return tree;

    /* Is this parent only alive to own this child? */
    if (!mallocator_should_destroy(parent)) return NULL;

    /* Destroy all ancestors */
    return mallocator_destroy_ancestors(parent);
}

static void mallocator_reference_int(mallocator_t *mallocator)
{
    mallocator->ref_count++;
}

static mallocator_tree_t *mallocator_dereference_int(mallocator_t *mallocator)
{
    mallocator->ref_count--;
    if (!mallocator_should_destroy(mallocator)) return NULL;

    /* Destroy all ancestors */
    return mallocator_destroy_ancestors(mallocator);
}

/**************************************************************************************************/
/* Public interface */

mallocator_t *mallocator_create(const char *name)
{
    return mallocator_create_int(name, NULL, NULL);
}

mallocator_t *mallocator_create_custom(const char *name, mallocator_impl_t *pimpl)
{
    return mallocator_create_int(name, pimpl, NULL);
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

void mallocator_reference(mallocator_t *mallocator)
{
    mallocator_verify(mallocator);

    mallocator_tree_lock(mallocator->tree);
    mallocator_reference_int(mallocator);
    mallocator_tree_unlock(mallocator->tree);
}

void mallocator_dereference(mallocator_t *mallocator)
{
    mallocator_verify(mallocator);

    mallocator_tree_t *tree = mallocator->tree;
    mallocator_tree_lock(tree);
    mallocator_tree_t *destroy_tree = mallocator_dereference_int(mallocator);
    mallocator_tree_unlock(tree);

    if (destroy_tree) mallocator_tree_destroy(destroy_tree);
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

    mallocator_tree_lock(mallocator->tree);
    mallocator_t *child = mallocator->children;
    if (child) mallocator_reference_int(child);
    mallocator_tree_unlock(mallocator->tree);
    return child;
}

mallocator_t *mallocator_child_next(mallocator_t *mallocator)
{
    mallocator_verify(mallocator);

    mallocator_tree_t *tree = mallocator->tree;
    mallocator_tree_lock(tree);
    mallocator_t *next = mallocator->next_child;
    if (next) mallocator_reference_int(next);
    mallocator_tree_t *destroy_tree = mallocator_dereference_int(mallocator);
    mallocator_tree_unlock(tree);

    if (destroy_tree) mallocator_tree_destroy(destroy_tree);
    return next;
}

mallocator_t *mallocator_child_lookup(mallocator_t *mallocator, const char *name)
{
    mallocator_verify(mallocator);

    mallocator_t *lookup = NULL;
    mallocator_tree_lock(mallocator->tree);
    for (mallocator_t *child = mallocator->children;
	 child != NULL;
	 child = child->next_child)
    {
	const int res = strcmp(child->name, name);
	if (res == 0)
	{
	    lookup = child;
	    mallocator_reference_int(child);
	    break;
	}
	else if (res > 0)
	{
	    break;
	}
    }
    mallocator_tree_unlock(mallocator->tree);
    return lookup;
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

    mallocator_stats_get(mallocator, stats);
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

    if (ptr)
    {
	mallocator_stats_allocated(mallocator, size);
    }
    else
    {
	mallocator_stats_failed(mallocator, size);
    }
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

    if (ptr)
    {
	mallocator_stats_allocated(mallocator, nmemb * size);
    }
    else
    {
	mallocator_stats_failed(mallocator, nmemb * size);
    }
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

    if ((!new_ptr || new_size > 0) && size > 0)
    {
	mallocator_stats_freed(mallocator, size);
    }
    if (new_size > 0)
    {
	if (new_ptr)
	{
	    mallocator_stats_allocated(mallocator, new_size);
	}
	else
	{
	    mallocator_stats_failed(mallocator, new_size);
	}
    }
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

    mallocator_stats_freed(mallocator, size);
}
