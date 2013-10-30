#include "mallocator_hs.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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

int main(int argc, char *argv[])
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
    return 0;
}
