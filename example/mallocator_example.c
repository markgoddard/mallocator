#define  _POSIX_C_SOURCE 200809L

#include "mallocator.h"
#include "mallocator_monkey.h"
#include "default_mallocator.h"
#include "module_mallocator.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void test_default_mallocator(void)
{
    mallocator_t *mallocator = mallocator_create("default");
    assert(!default_mallocator());
    default_mallocator_init(mallocator);
    mallocator_dereference(mallocator);
    assert(default_mallocator());
    void *ptr = default_mallocator_malloc(42);
    assert(ptr);
    default_mallocator_free(ptr, 42);
    default_mallocator_fini();
    assert(!default_mallocator());
    printf(".");
}

static void test_module_mallocator(void)
{
    mallocator_t *mallocator = mallocator_create("module");
    assert(!module_mallocator());
    module_mallocator_init(mallocator);
    mallocator_dereference(mallocator);
    assert(module_mallocator());
    void *ptr = module_mallocator_malloc(42);
    assert(ptr);
    module_mallocator_free(ptr, 42);
    module_mallocator_fini();
    assert(!module_mallocator());
    printf(".");
}

static void print_mallocator_stats_fn(void *arg, mallocator_t *mallocator)
{
    unsigned indent = (uintptr_t) arg;
    char indent_buf[16];
    memset(indent_buf, ' ', indent * 2);
    indent_buf[indent * 2] = '\0';

    mallocator_stats_t stats;
    mallocator_stats(mallocator, &stats);
    char name_buf[64];
    printf("%s%s:\n", indent_buf, mallocator_full_name(mallocator, name_buf, sizeof(name_buf)));
    printf("%sblocks allocated/freed/failed:\t%zu/%zu/%zu\n", indent_buf, stats.blocks_allocated, stats.blocks_freed, stats.blocks_failed);
    printf("%sbytes  allocated/freed/failed:\t%zu/%zu/%zu\n", indent_buf, stats.bytes_allocated, stats.bytes_freed, stats.bytes_failed);
    mallocator_iterate(mallocator, print_mallocator_stats_fn, (void *)(uintptr_t) (indent + 1));
}

static void print_mallocator_stats(mallocator_t *mallocator)
{
    print_mallocator_stats_fn((uintptr_t) 0, mallocator);
}

static void test_mallocator(void)
{
    mallocator_t *root = mallocator_create("root");

    for (unsigned i = 0; i < 42; i++)
    {
	int *int_array = mallocator_malloc(root, 4 * sizeof(int));
	mallocator_stats_t stats;
	mallocator_stats(root, &stats);
	assert(stats.blocks_allocated == i + 1);
	assert(stats.blocks_freed == i);
	assert(stats.bytes_allocated == (i + 1) * 4 * sizeof(int));
	assert(stats.bytes_freed == i * 4 * sizeof(int));

	mallocator_free(root, int_array, 4 * sizeof(int));
	mallocator_stats(root, &stats);
	assert(stats.blocks_allocated == i + 1);
	assert(stats.blocks_freed == i + 1);
	assert(stats.bytes_allocated == (i + 1) * 4 * sizeof(int));
	assert(stats.bytes_freed == (i + 1) * 4 * sizeof(int));
    }

    mallocator_t *child1 = mallocator_create_child(root, "child1");
    mallocator_t *child2 = mallocator_create_child(root, "child2");
    mallocator_t *grandchild = mallocator_create_child(child1, "grandchild");

    mallocator_dereference(root);

    int *int_array2 = mallocator_malloc(child1, 8 * sizeof(int));
    mallocator_free(child1, int_array2, 8 * sizeof(int));

    mallocator_dereference(child1);

    int *int_array3 = mallocator_malloc(child2, 16 * sizeof(int));
    mallocator_free(child2, int_array3, 16 * sizeof(int));

    mallocator_dereference(child2);

    int *int_array4 = mallocator_malloc(grandchild, 32 * sizeof(int));
    mallocator_free(grandchild, int_array4, 32 * sizeof(int));

    print_mallocator_stats(root);

    mallocator_dereference(grandchild);

    printf(".");
}

static void test_mallocator_monkey_random(void)
{
    srand(time(NULL));
    mallocator_monkey_t *monkey = mallocator_monkey_create_random(0.1, 0.1);
    mallocator_impl_t *impl = mallocator_monkey_impl(monkey);
    mallocator_t *mallocator = mallocator_create_custom("random", impl);
    for (unsigned i = 0; ; i++)
    {
	void *ptr = mallocator_malloc(mallocator, 1);
	if (!ptr)
	{
	    printf("Failure after %u successes\n", i);
	    break;
	}
	else
	{
	    mallocator_free(mallocator, ptr, 1);
	}
    }
    for (unsigned i = 0; ; i++)
    {
	void *ptr = mallocator_malloc(mallocator, 1);
	if (ptr)
	{
	    printf("Success after %u failures\n", i);
	    mallocator_free(mallocator, ptr, 1);
	    break;
	}
    }
    mallocator_dereference(mallocator);
    printf(".");
}

static void test_mallocator_monkey_step(void)
{
    mallocator_monkey_t *monkey = mallocator_monkey_create_step(20, 10, true);
    mallocator_impl_t *impl = mallocator_monkey_impl(monkey);
    mallocator_t *mallocator = mallocator_create_custom("step", impl);
    for (unsigned i = 0; i < 2; i++)
    {
	for (unsigned j = 0; j < 20; j++)
	{
	    void *ptr = mallocator_malloc(mallocator, 1);
	    assert(ptr);
	    mallocator_free(mallocator, ptr, 1);
	}
	for (unsigned j = 0; j < 10; j++)
	{
	    assert(!mallocator_malloc(mallocator, 1));
	}
    }
    mallocator_dereference(mallocator);
    printf(".");
}

static struct timespec time_diff(struct timespec a, struct timespec b)
{
    struct timespec diff;
    diff.tv_sec = a.tv_sec - b.tv_sec;
    if (a.tv_nsec > b.tv_nsec)
    {
	diff.tv_nsec = a.tv_nsec - b.tv_nsec;
    }
    else
    {
	diff.tv_nsec = 1000000000L + a.tv_nsec - b.tv_nsec;
	diff.tv_sec--;
    }
    return diff;
}

static void test_mallocator_performance(void)
{
    struct timespec start, end, t_malloc, t_mallocator; //, t_mallocator_hs;
    static void *ptrs[1000000];

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (unsigned i = 0; i < 1000000; i++)
    {
	ptrs[i] = malloc(256);
	assert(ptrs[i]);
    }
    for (unsigned i = 0; i < 1000000; i++)
    {
	free(ptrs[i]);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_malloc = time_diff(end, start);

    mallocator_t *mallocator = mallocator_create("performance");
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (unsigned i = 0; i < 1000000; i++)
    {
	ptrs[i] = mallocator_malloc(mallocator, 256);
	assert(ptrs[i]);
    }
    for (unsigned i = 0; i < 1000000; i++)
    {
	mallocator_free(mallocator, ptrs[i], 256);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    mallocator_dereference(mallocator);
    t_mallocator = time_diff(end, start);

    // TODO: With std impl
#if 0
    mallocator_hs_t *mallocator_hs = mallocator_hs_create("performance");
    mallocator = mallocator_hs_mallocator(mallocator_hs);
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (unsigned i = 0; i < 1000000; i++)
    {
	void *ptr = mallocator_malloc(mallocator, 256);
	assert(ptr);
	mallocator_free(mallocator, ptr, 256);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    mallocator_dereference(mallocator);
    t_mallocator_hs = time_diff(end, start);
#endif

    printf("malloc %d.%09lds\n", (int)t_malloc.tv_sec, (long int)t_malloc.tv_nsec);
    printf("mallocator %d.%09lds\n", (int)t_mallocator.tv_sec, (long int)t_mallocator.tv_nsec);
    //printf("mallocator_hs %d.%09lds\n", (int)t_mallocator_hs.tv_sec, (long int)t_mallocator_hs.tv_nsec);
}

int main(int argc, char *argv[])
{
    test_default_mallocator();
    test_module_mallocator();
    test_mallocator();
    test_mallocator_monkey_random();
    test_mallocator_monkey_step();
    test_mallocator_performance();
    printf("\n");
    return 0;
}
