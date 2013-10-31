#define  _POSIX_C_SOURCE 200809L

#include "mallocator_std.h"
#include "mallocator_hs.h"
#include "mallocator_monkey.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void print_mallocator_stats_fn(void *arg, mallocator_hs_t *mallocator)
{
    unsigned indent = (uintptr_t) arg;
    char indent_buf[16];
    memset(indent_buf, ' ', indent * 2);
    indent_buf[indent * 2] = '\0';

    mallocator_hs_stats_t stats;
    mallocator_hs_stats(mallocator, &stats);
    printf("%s%s:\n", indent_buf, mallocator_name(mallocator_hs_mallocator(mallocator)));
    printf("%sallocated blocks/bytes: %zu/%zu\n", indent_buf, stats.blocks_allocated, stats.bytes_allocated);
    printf("%sfreed     blocks/bytes: %zu/%zu\n", indent_buf, stats.blocks_freed, stats.bytes_freed);
    printf("%sfailed    blocks/bytes: %zu/%zu\n", indent_buf, stats.blocks_failed, stats.bytes_failed);
    mallocator_hs_iterate(mallocator, print_mallocator_stats_fn, (void *)(uintptr_t) (indent + 1));
}

static void print_mallocator_stats(mallocator_hs_t *mallocator)
{
    print_mallocator_stats_fn((uintptr_t) 0, mallocator);
}

static void test_mallocator_hs(void)
{
    mallocator_hs_t *root_hs = mallocator_hs_create("root");
    mallocator_t *root = mallocator_hs_mallocator(root_hs);

    for (unsigned i = 0; i < 42; i++)
    {
	int *int_array = mallocator_malloc(root, 4 * sizeof(int));
	mallocator_hs_stats_t stats;
	mallocator_hs_stats(root_hs, &stats);
	assert(stats.blocks_allocated == i + 1);
	assert(stats.blocks_freed == i);
	assert(stats.bytes_allocated == (i + 1) * 4 * sizeof(int));
	assert(stats.bytes_freed == i * 4 * sizeof(int));

	mallocator_free(root, int_array, 4 * sizeof(int));
	mallocator_hs_stats(root_hs, &stats);
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

    print_mallocator_stats(root_hs);

    mallocator_dereference(grandchild);

    printf(".");
}

static void test_mallocator_monkey_random(void)
{
    srand(time(NULL));
    mallocator_monkey_t *mallocator_impl = mallocator_monkey_create_random("example", 0.1, 0.1);
    mallocator_t *mallocator = mallocator_monkey_mallocator(mallocator_impl);
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
    printf(".");
}

static void test_mallocator_monkey_step(void)
{
    mallocator_monkey_t *mallocator_impl = mallocator_monkey_create_step("example", 20, 10, true);
    mallocator_t *mallocator = mallocator_monkey_mallocator(mallocator_impl);
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
    struct timespec start, end, t_malloc, t_mallocator, t_mallocator_hs;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (unsigned i = 0; i < 1000000; i++)
    {
	void *ptr = malloc(256);
	assert(ptr);
	free(ptr);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_malloc = time_diff(end, start);

    mallocator_std_t *mallocator_std = mallocator_std_create("performance");
    mallocator_t *mallocator = mallocator_std_mallocator(mallocator_std);
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (unsigned i = 0; i < 1000000; i++)
    {
	void *ptr = mallocator_malloc(mallocator, 256);
	assert(ptr);
	mallocator_free(mallocator, ptr, 256);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    mallocator_dereference(mallocator);
    t_mallocator = time_diff(end, start);

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

    printf("malloc %d.%09lds\n", (int)t_malloc.tv_sec, (long int)t_malloc.tv_nsec);
    printf("mallocator_std %d.%09lds\n", (int)t_mallocator.tv_sec, (long int)t_mallocator.tv_nsec);
    printf("mallocator_hs %d.%09lds\n", (int)t_mallocator_hs.tv_sec, (long int)t_mallocator_hs.tv_nsec);
}

int main(int argc, char *argv[])
{
    test_mallocator_hs();
    test_mallocator_monkey_random();
    test_mallocator_monkey_step();
    test_mallocator_performance();
    printf("\n");
    return 0;
}
