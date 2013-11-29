#include "mallocator_tracer.h"
#include "mallocator.h"
#include "mallocator_impl.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

typedef struct
{
    mallocator_impl_t impl;
    pthread_mutex_t lock;
    unsigned ref_count;		/* Protected by lock */
    mallocator_tracer_fn fn;
    void *arg;
} mallocator_tracer_t;

/**************************************************************************************************/
/* mallocator_t interface */

static mallocator_impl_t *mallocator_tracer_create_child(void *parent_obj, const char *name);
static void mallocator_tracer_destroy(void *obj);
static void *mallocator_tracer_malloc(void *obj, size_t size);
static void *mallocator_tracer_calloc(void *obj, size_t nmemb, size_t size);
static void *mallocator_tracer_realloc(void *obj, void *ptr, size_t size, size_t new_size);
static void mallocator_tracer_free(void *obj, void *p, size_t size);

static mallocator_interface_t mallocator_tracer_interface =
{
    .create_child = mallocator_tracer_create_child,
    .destroy = mallocator_tracer_destroy,
    .malloc = mallocator_tracer_malloc,
    .calloc = mallocator_tracer_calloc,
    .realloc = mallocator_tracer_realloc,
    .free = mallocator_tracer_free,
};

/**************************************************************************************************/
/* Event utils */

const char *mallocator_tracer_type_str(mallocator_tracer_type_t type)
{
    switch (type)
    {
	case MALLOCATOR_TRACER_MALLOC: return "malloc";
	case MALLOCATOR_TRACER_CALLOC: return "calloc";
	case MALLOCATOR_TRACER_REALLOC: return "realloc";
	case MALLOCATOR_TRACER_FREE: return "free";
	default: return "<unknown>";
    }
}

/**************************************************************************************************/

static inline mallocator_tracer_t *mallocator_tracer_verify(void *obj)
{
    mallocator_tracer_t *mallocator = obj;
    assert(mallocator);
    assert(mallocator->ref_count > 0);
    return mallocator;
}

static void mallocator_tracer_init(mallocator_tracer_t *mallocator, mallocator_tracer_fn fn, void *arg)
{
    *mallocator = (mallocator_tracer_t)
    {
	.impl =
	{
	    .obj = mallocator,
	    .interface = &mallocator_tracer_interface,
	},
	.ref_count = 1,
	.fn = fn,
	.arg = arg,
    };
    assert(pthread_mutex_init(&mallocator->lock, NULL) == 0);
}

static void mallocator_tracer_fini(mallocator_tracer_t *mallocator)
{
    assert(pthread_mutex_destroy(&mallocator->lock) == 0);
    *mallocator = (mallocator_tracer_t)
    {
	.impl =
	{
	    .obj = NULL,
	    .interface = NULL,
	},
	.ref_count = 0,
	.fn = NULL,
	.arg = NULL,
    };
}

static inline void mallocator_tracer_lock(mallocator_tracer_t *mallocator)
{
    assert(pthread_mutex_lock(&mallocator->lock) == 0);
}

static inline void mallocator_tracer_unlock(mallocator_tracer_t *mallocator)
{
    assert(pthread_mutex_unlock(&mallocator->lock) == 0);
}

static mallocator_tracer_t *mallocator_tracer_create_int(mallocator_tracer_fn fn, void *arg)
{
    mallocator_tracer_t *mallocator = malloc(sizeof(*mallocator));
    if (!mallocator) return NULL;

    mallocator_tracer_init(mallocator, fn, arg);
    return mallocator;
}

static void mallocator_tracer_reference(mallocator_tracer_t *mallocator)
{
    mallocator_tracer_lock(mallocator);
    mallocator->ref_count++;
    mallocator_tracer_unlock(mallocator);
}

static void mallocator_tracer_dereference(mallocator_tracer_t *mallocator)
{
    mallocator_tracer_lock(mallocator);
    mallocator->ref_count--;
    const bool destroy = mallocator->ref_count == 0;
    mallocator_tracer_unlock(mallocator);

    if (destroy)
    {
	mallocator_tracer_fini(mallocator);
	free(mallocator);
    }
}

static mallocator_impl_t *mallocator_tracer_create_child(void *parent_obj, const char *name)
{
    mallocator_tracer_t *parent = mallocator_tracer_verify(parent_obj);
    mallocator_tracer_reference(parent);
    return &parent->impl;
}

static void mallocator_tracer_destroy(void *obj)
{
    mallocator_tracer_t *mallocator = mallocator_tracer_verify(obj);
    mallocator_tracer_dereference(mallocator);
}

static unsigned mallocator_backtrace(mallocator_tracer_t *mallocator, void **backtrace)
{
    /* NOTE: Argument must be a constant integer */
    backtrace[0] = __builtin_return_address(0);
    backtrace[1] = __builtin_return_address(1);
    backtrace[2] = __builtin_return_address(2);
    backtrace[3] = __builtin_return_address(3);
    backtrace[4] = __builtin_return_address(4);
    backtrace[5] = __builtin_return_address(5);
    backtrace[6] = __builtin_return_address(6);
    backtrace[7] = __builtin_return_address(7);
    unsigned len = 0;
    for (unsigned i = 0; i < MALLOCATOR_TRACER_BACKTRACE_MAX; i++)
    {
	backtrace[i] = __builtin_extract_return_addr(backtrace[i]);
	if (!backtrace[i]) break;
	len++;
    }
    return len;
}

static void mallocator_event(mallocator_tracer_t *mallocator, mallocator_tracer_event_t *event)
{
    mallocator->fn(mallocator->arg, event);
}

static void *mallocator_tracer_malloc(void *obj, size_t size)
{
    mallocator_tracer_t *mallocator = mallocator_tracer_verify(obj);
    void *ptr = malloc(size);
    mallocator_tracer_event_t event =
    {
	.type = MALLOCATOR_TRACER_MALLOC,
	.ptr = ptr,
	.e.malloc =
	{
	    .size = size,
	},
    };
    event.backtrace_len = mallocator_backtrace(mallocator, event.backtrace);
    mallocator_event(mallocator, &event);
    return ptr;
}

static void *mallocator_tracer_calloc(void *obj, size_t nmemb, size_t size)
{
    mallocator_tracer_t *mallocator = mallocator_tracer_verify(obj);
    void *ptr = calloc(nmemb, size);
    mallocator_tracer_event_t event =
    {
	.type = MALLOCATOR_TRACER_CALLOC,
	.ptr = ptr,
	.e.calloc =
	{
	    .nmemb = nmemb,
	    .size = size,
	},
    };
    event.backtrace_len = mallocator_backtrace(mallocator, event.backtrace);
    mallocator_event(mallocator, &event);
    return ptr;
}

static void *mallocator_tracer_realloc(void *obj, void *ptr, size_t size, size_t new_size)
{
    mallocator_tracer_t *mallocator = mallocator_tracer_verify(obj);
    void *new_ptr = realloc(ptr, new_size);
    mallocator_tracer_event_t event =
    {
	.type = MALLOCATOR_TRACER_REALLOC,
	.ptr = new_ptr,
	.e.realloc =
	{
	    .old_ptr = ptr,
	    .old_size = size,
	    .new_size = new_size,
	},
    };
    event.backtrace_len = mallocator_backtrace(mallocator, event.backtrace);
    mallocator_event(mallocator, &event);
    return new_ptr;
}

static void mallocator_tracer_free(void *obj, void *ptr, size_t size)
{
    mallocator_tracer_t *mallocator = mallocator_tracer_verify(obj);
    mallocator_tracer_event_t event =
    {
	.type = MALLOCATOR_TRACER_FREE,
	.ptr = ptr,
	.e.free =
	{
	    .size = size,
	},
    };
    event.backtrace_len = mallocator_backtrace(mallocator, event.backtrace);
    mallocator_event(mallocator, &event);
    free(ptr);
}

/**************************************************************************************************/
/* Public interface */

mallocator_t *mallocator_tracer_create(const char *name, mallocator_tracer_fn fn, void *arg)
{
    mallocator_tracer_t *tracer = mallocator_tracer_create_int(fn, arg);
    if (!tracer) return NULL;

    mallocator_t *mallocator = mallocator_create_custom(name, &tracer->impl);
    if (!mallocator)
    {
	mallocator_tracer_destroy(&tracer->impl);
	return NULL;
    }
    return mallocator;
}
