#include "mallocator.h"
#include "mallocator_interface.h"
#include "mallocator_monkey.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

typedef struct
{
    float p_failure;	/* Probability of failure (0-1) */
    float p_recovery;	/* Probability of recovery during failure (0-1) */
    bool failing;
} mallocator_monkey_random_t;

typedef struct
{
    unsigned num_success;   /* Number of successful allocations before a failure */
    unsigned num_failure;   /* Number of unsuccessful allocations (0 for infinite) */
    bool repeat;	    /* Whether to repeat the sequence after the final failure */
    unsigned count;
    bool failing;
    bool failed;
} mallocator_monkey_step_t;

struct mallocator_monkey
{
    mallocator_t mallocator;
    pthread_mutex_t lock;	    /* Protects ref_count */
    unsigned ref_count;
    const char *name;
    mallocator_monkey_fail_fn fn;
    void *arg;
    union
    {
	mallocator_monkey_random_t random;
	mallocator_monkey_step_t step;
    } chaos;
};

/**************************************************************************************************/
/* mallocator_t interface */

static mallocator_t *mallocator_monkey_create_child(void *parent_obj, const char *name);
static void mallocator_monkey_reference(void *obj);
static void mallocator_monkey_dereference(void *obj);
static const char *mallocator_monkey_name(void *obj);
static void *mallocator_monkey_malloc(void *obj, size_t size);
static void *mallocator_monkey_calloc(void *obj, size_t nmemb, size_t size);
static void *mallocator_monkey_realloc(void *obj, void *ptr, size_t size, size_t new_size);
static void mallocator_monkey_free(void *obj, void *p, size_t size);

static mallocator_interface_t mallocator_monkey_interface =
{
    .create_child = mallocator_monkey_create_child,
    .reference = mallocator_monkey_reference,
    .dereference = mallocator_monkey_dereference,
    .name = mallocator_monkey_name,
    .malloc = mallocator_monkey_malloc,
    .calloc = mallocator_monkey_calloc,
    .realloc = mallocator_monkey_realloc,
    .free = mallocator_monkey_free,
};

/**************************************************************************************************/

static inline mallocator_monkey_t *mallocator_monkey_verify(void *obj)
{
    mallocator_monkey_t *mallocator = obj;
    assert(mallocator);
    assert(mallocator->ref_count > 0);
    return mallocator;
}

static void mallocator_monkey_init(mallocator_monkey_t *mallocator, const char *name)
{
    *mallocator = (mallocator_monkey_t)
    {
	.mallocator =
	{
	    .obj = mallocator,
	    .interface = &mallocator_monkey_interface,
	},
	.ref_count = 1,
	.name = name,
	.fn = NULL,
	.arg = NULL,
    };
    assert(pthread_mutex_init(&mallocator->lock, NULL) == 0);
}

static void mallocator_monkey_fini(mallocator_monkey_t *mallocator)
{
    assert(pthread_mutex_destroy(&mallocator->lock) == 0);
    *mallocator = (mallocator_monkey_t)
    {
	.mallocator =
	{
	    .obj = NULL,
	    .interface = NULL,
	},
	.ref_count = 0,
	.name = NULL,
	.fn = NULL,
	.arg = NULL,
    };
}

static inline void mallocator_monkey_lock(mallocator_monkey_t *mallocator)
{
    assert(pthread_mutex_lock(&mallocator->lock) == 0);
}

static inline void mallocator_monkey_unlock(mallocator_monkey_t *mallocator)
{
    assert(pthread_mutex_unlock(&mallocator->lock) == 0);
}

static mallocator_monkey_t *mallocator_monkey_create_int(const char *name)
{
    mallocator_monkey_t *mallocator = malloc(sizeof(*mallocator));
    if (!mallocator) return NULL;

    mallocator_monkey_init(mallocator, name);
    return mallocator;
}

static void mallocator_monkey_destroy(mallocator_monkey_t *mallocator)
{
    mallocator_monkey_fini(mallocator);
    free(mallocator);
}

static mallocator_t *mallocator_monkey_create_child(void *parent_obj, const char *name)
{
    mallocator_monkey_t *parent = mallocator_monkey_verify(parent_obj);
    mallocator_monkey_reference(parent);
    return &parent->mallocator;
}

static void mallocator_monkey_reference(void *obj)
{
    mallocator_monkey_t *mallocator = mallocator_monkey_verify(obj);

    mallocator_monkey_lock(mallocator);
    mallocator->ref_count++;
    mallocator_monkey_unlock(mallocator);
}

static void mallocator_monkey_dereference(void *obj)
{
    mallocator_monkey_t *mallocator = mallocator_monkey_verify(obj);

    mallocator_monkey_lock(mallocator);
    mallocator->ref_count--;
    const bool destroy = mallocator->ref_count == 0;
    mallocator_monkey_unlock(mallocator);

    if (destroy)
	mallocator_monkey_destroy(mallocator);
}

static const char *mallocator_monkey_name(void *obj)
{
    mallocator_monkey_t *mallocator = mallocator_monkey_verify(obj);
    return mallocator->name;
}

static bool mallocator_monkey_fail_random(void *arg)
{
    mallocator_monkey_random_t *random = arg;
    double p = rand() / (float)RAND_MAX;
    if (random->failing)
    {
	if (p < random->p_recovery)
	    random->failing = false;
    }
    else
    {
	if (p < random->p_failure)
	    random->failing = true;
    }
    return random->failing;
}

static bool mallocator_monkey_fail_step(void *arg)
{
    mallocator_monkey_step_t *step = arg;
    step->count++;
    if (step->failing)
    {
	if (step->count > step->num_failure)
	{
	    step->failing = false;
	    step->count = 1;
	}
    }
    else if (!step->failed || step->repeat)
    {
	if (step->count > step->num_success)
	{
	    step->failing = true;
	    step->failed = true;
	    step->count = 1;
	}
    }
    return step->failing;
}

static inline bool mallocator_monkey_fail(mallocator_monkey_t *mallocator)
{
    return mallocator->fn(mallocator->arg);
}

static void *mallocator_monkey_malloc(void *obj, size_t size)
{
    mallocator_monkey_t *mallocator = mallocator_monkey_verify(obj);
    if (mallocator_monkey_fail(mallocator))
	return NULL;

    return malloc(size);
}

static void *mallocator_monkey_calloc(void *obj, size_t nmemb, size_t size)
{
    mallocator_monkey_t *mallocator = mallocator_monkey_verify(obj);
    if (mallocator_monkey_fail(mallocator))
	return NULL;

    return calloc(nmemb, size);
}

static void *mallocator_monkey_realloc(void *obj, void *ptr, size_t size, size_t new_size)
{
    mallocator_monkey_t *mallocator = mallocator_monkey_verify(obj);
    if (mallocator_monkey_fail(mallocator))
	return NULL;

    return realloc(ptr, new_size);
}

static void mallocator_monkey_free(void *obj, void *ptr, size_t size)
{
    mallocator_monkey_t *mallocator = mallocator_monkey_verify(obj);
    free(ptr);
}

/**************************************************************************************************/
/* Public interface */

mallocator_monkey_t *mallocator_monkey_create_random(const char *name, float p_failure, float p_recovery)
{
    mallocator_monkey_t *mallocator = mallocator_monkey_create_int(name);
    if (!mallocator) return NULL;
    mallocator->chaos.random.p_failure = p_failure;
    mallocator->chaos.random.p_recovery = p_recovery;
    mallocator->chaos.random.failing = false;
    mallocator->fn = mallocator_monkey_fail_random;
    mallocator->arg = &mallocator->chaos.random;
    return mallocator;
}

mallocator_monkey_t *mallocator_monkey_create_step(const char *name, unsigned num_success, unsigned num_failure, bool repeat)
{
    mallocator_monkey_t *mallocator = mallocator_monkey_create_int(name);
    if (!mallocator) return NULL;
    mallocator->chaos.step.num_success = num_success;
    mallocator->chaos.step.num_failure = num_failure;
    mallocator->chaos.step.repeat = repeat;
    mallocator->chaos.step.count = 0;
    mallocator->chaos.step.failing = false;
    mallocator->chaos.step.failed = false;
    mallocator->fn = mallocator_monkey_fail_step;
    mallocator->arg = &mallocator->chaos.step;
    return mallocator;
}

mallocator_monkey_t *mallocator_monkey_create_custom(const char *name, mallocator_monkey_fail_fn fn, void *arg)
{
    mallocator_monkey_t *mallocator = mallocator_monkey_create_int(name);
    if (!mallocator) return NULL;
    mallocator->fn = fn;
    mallocator->arg = arg;
    return mallocator;
}

mallocator_t *mallocator_monkey_mallocator(mallocator_monkey_t *mallocator)
{
    return &mallocator->mallocator;
}
