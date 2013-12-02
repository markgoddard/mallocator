#include <cgreen/cgreen.h>

#include "mallocator.h"

#include <malloc.h>

Describe(mallocator);

static mallocator_t *m;
static struct mallinfo mallinfo_before;

BeforeEach(mallocator) 
{
    mallinfo_before = mallinfo();

    m = mallocator_create("test");
    assert_that(m, is_non_null);
}

AfterEach(mallocator)
{
    assert_that(m, is_non_null);
    mallocator_dereference(m);

    /* Ensure that there are no memory leaks */
    struct mallinfo mallinfo_after = mallinfo();
    assert_that(mallinfo_after.uordblks, is_equal_to(mallinfo_before.uordblks));
    assert_that(mallinfo_after.fordblks, is_equal_to(mallinfo_before.fordblks));
}

Ensure(mallocator, can_be_referenced)
{
    for (unsigned num_refs = 0; num_refs < 10; num_refs++)
    {
	mallocator_reference(m);
	mallocator_dereference(m);
    }
}

Ensure(mallocator, can_be_multiply_referenced)
{
    for (unsigned num_refs = 0; num_refs < 10; num_refs++)
    {
	mallocator_reference(m);
    }
    for (unsigned num_refs = 0; num_refs < 10; num_refs++)
    {
	mallocator_dereference(m);
    }
}

Ensure(mallocator, has_a_name)
{
    const char *name = mallocator_name(m);
    assert_that(name, is_equal_to_string("test"));
}

Ensure(mallocator, has_a_full_name)
{
    char name_buf[8];
    const char *name = mallocator_full_name(m, name_buf, sizeof(name_buf));
    assert_that(name, is_equal_to_string("test"));
}

static void count_mallocators(void *arg, mallocator_t *m)
{
    unsigned *count = arg;
    (*count)++;
}

Ensure(mallocator, has_no_family)
{
    assert_that(mallocator_child_begin(m), is_null);

    mallocator_reference(m);	/* Next dereferences m */
    assert_that(mallocator_child_next(m), is_null);

    unsigned count = 0;
    mallocator_iterate(m, count_mallocators, &count);
    assert_that(count, is_equal_to(0));
}

Ensure(mallocator, can_create_children)
{
    mallocator_t *child = mallocator_create_child(m, "child");
    assert_that(child, is_non_null);
    assert_that(mallocator_child_begin(m), is_equal_to(child));
    assert_that(mallocator_child_next(child), is_null);
    mallocator_dereference(child);
}

Ensure(mallocator, can_create_multiple_children)
{
    unsigned num_children = 10;
    char names[10][8];
    mallocator_t *child[10];
    for (unsigned i = 0; i < num_children; i++)
    {
	snprintf(names[i], sizeof(names[i]), "child%u", i);
	child[i] = mallocator_create_child(m, names[i]);
	assert_that(child[i], is_non_null);
    }
    mallocator_t *curr = mallocator_child_begin(m);
    for (unsigned i = 0; i < num_children; i++)
    {
	/* Should be iterated in order */
	assert_that(curr, is_equal_to(child[i]));
	curr = mallocator_child_next(curr);
    }
    assert_that(curr, is_null);
    for (unsigned i = 0; i < num_children; i++)
    {
	mallocator_dereference(child[i]);
    }
}

Ensure(mallocator, can_create_multiple_generations)
{
    unsigned num_children = 4;
    char names1[num_children][16];
    mallocator_t *gen1[num_children];
    char names2[num_children][num_children][16];
    mallocator_t *gen2[num_children][num_children];
    char names3[num_children][num_children][num_children][16];
    mallocator_t *gen3[num_children][num_children][num_children];
    for (unsigned c = 0; c < num_children; c++)
    {
	snprintf(names1[c], sizeof(names1[c]), "gen1:%u", c);
	gen1[c] = mallocator_create_child(m, names1[c]);
	assert_that(gen1[c], is_non_null);
	assert_that(gen1[c], is_not_equal_to(m));
	for (unsigned gc = 0; gc < num_children; gc++)
	{
	    snprintf(names2[c][gc], sizeof(names2[c][gc]), "gen2:%u,%u", c, gc);
	    gen2[c][gc] = mallocator_create_child(gen1[c], names2[c][gc]);
	    assert_that(gen2[c][gc], is_non_null);
	    assert_that(gen2[c][gc], is_not_equal_to(gen1[c]));
	    for (unsigned ggc = 0; ggc < num_children; ggc++)
	    {
		snprintf(names3[c][gc][ggc], sizeof(names3[c][gc][ggc]), "gen3:%u,%u,%u", c, gc, ggc);
		gen3[c][gc][ggc] = mallocator_create_child(gen2[c][gc], names3[c][gc][ggc]);
		assert_that(gen3[c][gc][ggc], is_non_null);
		assert_that(gen3[c][gc][ggc], is_not_equal_to(gen2[c][gc]));
	    }
	}
    }
    mallocator_t *curr1 = mallocator_child_begin(m);
    for (unsigned c = 0; c < num_children; c++)
    {
	/* Should be iterated in order */
	assert_that(curr1, is_equal_to(gen1[c]));
	mallocator_t *curr2 = mallocator_child_begin(curr1);
	for (unsigned gc = 0; gc < num_children; gc++)
	{
	    assert_that(curr2, is_equal_to(gen2[c][gc]));
	    mallocator_t *curr3 = mallocator_child_begin(curr2);
	    for (unsigned ggc = 0; ggc < num_children; ggc++)
	    {
		assert_that(curr3, is_equal_to(gen3[c][gc][ggc]));
		curr3 = mallocator_child_next(curr3);
	    }
	    curr2 = mallocator_child_next(curr2);
	}
	curr1 = mallocator_child_next(curr1);
    }
    assert_that(curr1, is_null);
    for (unsigned c = 0; c < num_children; c++)
    {
	for (unsigned gc = 0; gc < num_children; gc++)
	{
	    for (unsigned ggc = 0; ggc < num_children; ggc++)
	    {
		mallocator_dereference(gen3[c][gc][ggc]);
	    }
	    mallocator_dereference(gen2[c][gc]);
	}
	mallocator_dereference(gen1[c]);
    }
}

Ensure(mallocator, references_children)
{
    unsigned num_generations = 4;
    char names[num_generations - 1][8];
    mallocator_t *family[num_generations];
    family[0] = m;
    for (unsigned generation = 1; generation < num_generations; generation++)
    {
	snprintf(names[generation], sizeof(names[generation]), "gen%u", generation);
	family[generation] = mallocator_create_child(family[generation - 1], names[generation]);
	assert_that(family[generation], is_non_null);
    }
    for (unsigned generation = 0; generation < num_generations - 1; generation++)
    {
	assert_that(mallocator_child_begin(family[generation]), is_equal_to(family[generation + 1]));
	assert_that(mallocator_child_next(family[generation + 1]), is_null);
    }
    for (unsigned generation = 1; generation < num_generations; generation++)
    {
	mallocator_dereference(family[generation]);
    }
}

static void check_names(void *obj, mallocator_t *mallocator)
{
    mallocator_t **last_p = obj;
    if (*last_p)
    {
	int r = strcmp(mallocator_name(mallocator), mallocator_name(*last_p));
	assert_that(r, is_greater_than(0));
    }
    *last_p = mallocator;
}

Ensure(mallocator, iterates_children_in_order)
{
    unsigned num_children = 4;
    const char *names[4] = { "a", "aaa", "bcd", "zyx" };
    unsigned order[4] = { 2, 1, 3, 0 };
    mallocator_t *children[num_children];
    for (unsigned i = 0; i < num_children; i++)
    {
	children[i] = mallocator_create_child(m, names[order[i]]);
	assert_that(children[i], is_non_null);
    }
    mallocator_t *curr = mallocator_child_begin(m);
    for (unsigned i = 0; i < num_children; i++)
    {
	assert_that(mallocator_name(curr), is_equal_to_string(names[i]));
	curr = mallocator_child_next(curr);
    }
    assert_that(curr, is_null);
    mallocator_t *last = NULL;
    mallocator_iterate(m, check_names, &last);
    for (unsigned i = 0; i < num_children; i++)
    {
	mallocator_dereference(children[i]);
    }
}

Ensure(mallocator, can_lookup_children)
{
    unsigned num_children = 4;
    const char *names[4] = { "foo", "bar", "baz", "foobar" };
    mallocator_t *children[num_children];
    for (unsigned i = 0; i < num_children; i++)
    {
	children[i] = mallocator_create_child(m, names[i]);
    }
    for (unsigned i = 0; i < num_children; i++)
    {
	mallocator_t *lookup = mallocator_child_lookup(m, names[i]);
	assert_that(lookup, is_equal_to(children[i]));
	mallocator_dereference(lookup);
    }
    for (unsigned i = 0; i < num_children; i++)
    {
	mallocator_dereference(children[i]);
    }
}

Ensure(mallocator, references_iterated_children)
{
    unsigned num_children = 4;
    const char *names[4] = { "1", "2", "3", "4" };
    mallocator_t *children[num_children];
    for (unsigned i = 0; i < num_children; i++)
    {
	children[i] = mallocator_create_child(m, names[i]);
    }
    for (unsigned i = 0; i < num_children; i++)
    {
	mallocator_t *curr = mallocator_child_begin(m);
	for (unsigned j = 0; j < i; j++)
	{
	    curr = mallocator_child_next(curr);
	}
	mallocator_dereference(curr);
    }
    for (unsigned i = 0; i < num_children; i++)
    {
	mallocator_dereference(children[i]);
    }
}

Ensure(mallocator, children_are_uniquely_named)
{
    mallocator_t *child = mallocator_create_child(m, "child");
    assert_that(mallocator_create_child(m, "child"), is_null);
    mallocator_dereference(child);
}

Ensure(mallocator, can_malloc)
{
    unsigned num = 1024;
    int *ints = mallocator_malloc(m, num * sizeof(int));
    assert_that(ints, is_non_null);
    for (unsigned i = 0; i < num; i++)
    {
	ints[i] = i;
    }
    mallocator_free(m, ints, num * sizeof(int));
}

Ensure(mallocator, can_calloc)
{
    unsigned num = 1024;
    int *ints = mallocator_calloc(m, num, sizeof(int));
    assert_that(ints, is_non_null);
    for (unsigned i = 0; i < num; i++)
    {
	assert_that(ints[i], is_equal_to(0));
	ints[i] = i;
    }
    mallocator_free(m, ints, num * sizeof(int));
}

Ensure(mallocator, can_realloc)
{
    unsigned num = 1024;
    int *ints = mallocator_realloc(m, NULL, 0, num * sizeof(int));
    assert_that(ints, is_non_null);
    for (unsigned i = 0; i < num; i++)
    {
	ints[i] = i;
    }
    unsigned more_num = 10 * num;
    int *more_ints = mallocator_realloc(m, ints, num * sizeof(int), more_num * sizeof(int));
    assert_that(more_ints, is_non_null);
    for (unsigned i = 0; i < more_num; i++)
    {
	if (i < num) assert_that(ints[i], is_equal_to(i));
	else ints[i] = i;
    }
    int *no_ints = mallocator_realloc(m, more_ints, more_num * sizeof(int), 0);
    assert_that(no_ints, is_null);
}

Ensure(mallocator, counts_malloc)
{
    unsigned num = 1024;
    mallocator_stats_t stats;
    for (unsigned i = 0; i < 10; i++)
    {
	int *ints = mallocator_malloc(m, num * sizeof(int));
	assert_that(ints, is_non_null);
	mallocator_stats(m, &stats);
	assert_that(stats.blocks_allocated, is_equal_to(i + 1));
	assert_that(stats.blocks_freed, is_equal_to(i));
	assert_that(stats.bytes_allocated, is_equal_to((i + 1) * num * sizeof(int)));
	assert_that(stats.bytes_freed, is_equal_to(i * num * sizeof(int)));

	mallocator_free(m, ints, num * sizeof(int));
	mallocator_stats(m, &stats);
	assert_that(stats.blocks_allocated, is_equal_to(i + 1));
	assert_that(stats.blocks_freed, is_equal_to(i + 1));
	assert_that(stats.bytes_allocated, is_equal_to((i + 1) * num * sizeof(int)));
	assert_that(stats.bytes_freed, is_equal_to((i + 1) * num * sizeof(int)));
    }
}

Ensure(mallocator, counts_calloc)
{
    unsigned num = 1024;
    mallocator_stats_t stats;
    for (unsigned i = 0; i < 10; i++)
    {
	int *ints = mallocator_calloc(m, num, sizeof(int));
	assert_that(ints, is_non_null);
	mallocator_stats(m, &stats);
	assert_that(stats.blocks_allocated, is_equal_to(i + 1));
	assert_that(stats.blocks_freed, is_equal_to(i));
	assert_that(stats.bytes_allocated, is_equal_to((i + 1) * num * sizeof(int)));
	assert_that(stats.bytes_freed, is_equal_to(i * num * sizeof(int)));

	mallocator_free(m, ints, num * sizeof(int));
	mallocator_stats(m, &stats);
	assert_that(stats.blocks_allocated, is_equal_to(i + 1));
	assert_that(stats.blocks_freed, is_equal_to(i + 1));
	assert_that(stats.bytes_allocated, is_equal_to((i + 1) * num * sizeof(int)));
	assert_that(stats.bytes_freed, is_equal_to((i + 1) * num * sizeof(int)));
    }
}

Ensure(mallocator, counts_realloc)
{
    unsigned num = 1024;
    mallocator_stats_t stats;
    for (unsigned i = 0; i < 10; i++)
    {
	int *ints = mallocator_realloc(m, NULL, 0, num * sizeof(int));
	assert_that(ints, is_non_null);
	mallocator_stats(m, &stats);
	assert_that(stats.blocks_allocated, is_equal_to(2 * i + 1));
	assert_that(stats.blocks_freed, is_equal_to(2 * i));
	assert_that(stats.bytes_allocated, is_equal_to((3 * i + 1) * num * sizeof(int)));
	assert_that(stats.bytes_freed, is_equal_to(3 * i * num * sizeof(int)));

	int *ints2 = mallocator_realloc(m, ints, num * sizeof(int), 2 * num * sizeof(int));
	assert_that(ints2, is_non_null);
	mallocator_stats(m, &stats);
	assert_that(stats.blocks_allocated, is_equal_to(2 * i + 2));
	assert_that(stats.blocks_freed, is_equal_to(2 * i + 1));
	assert_that(stats.bytes_allocated, is_equal_to((3 * i + 3) * num * sizeof(int)));
	assert_that(stats.bytes_freed, is_equal_to((3 * i + 1) * num * sizeof(int)));

	mallocator_realloc(m, ints2, 2 * num * sizeof(int), 0);
	mallocator_stats(m, &stats);
	assert_that(stats.blocks_allocated, is_equal_to(2 * i + 2));
	assert_that(stats.blocks_freed, is_equal_to(2 * i + 2));
	assert_that(stats.bytes_allocated, is_equal_to((3 * i + 3) * num * sizeof(int)));
	assert_that(stats.bytes_freed, is_equal_to((3 * i + 3) * num * sizeof(int)));
    }
}

TestSuite *mallocator_tests(void)
{
    TestSuite *suite = create_test_suite();
    add_test_with_context(suite, mallocator, can_be_referenced);
    add_test_with_context(suite, mallocator, can_be_multiply_referenced);
    add_test_with_context(suite, mallocator, has_a_name);
    add_test_with_context(suite, mallocator, has_a_full_name);
    add_test_with_context(suite, mallocator, has_no_family);
    add_test_with_context(suite, mallocator, can_create_children);
    add_test_with_context(suite, mallocator, can_create_multiple_generations);
    add_test_with_context(suite, mallocator, references_children);
    add_test_with_context(suite, mallocator, iterates_children_in_order);
    add_test_with_context(suite, mallocator, can_lookup_children);
    add_test_with_context(suite, mallocator, references_iterated_children);
    add_test_with_context(suite, mallocator, children_are_uniquely_named);
    add_test_with_context(suite, mallocator, can_malloc);
    add_test_with_context(suite, mallocator, can_calloc);
    add_test_with_context(suite, mallocator, can_realloc);
    add_test_with_context(suite, mallocator, counts_malloc);
    add_test_with_context(suite, mallocator, counts_calloc);
    add_test_with_context(suite, mallocator, counts_realloc);
    return suite;
}
