#include <cgreen/cgreen.h>

#include "mallocator.h"
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
    mallocator_dereference(child);
}

Ensure(mallocator, can_create_multiple_children)
{
    char names[10][8];
    mallocator_t *child[10];
    for (unsigned i = 0; i < 10; i++)
    {
	snprintf(names[i], sizeof(names[i]), "child%u", i);
	child[i] = mallocator_create_child(m, names[i]);
	assert_that(child[i], is_non_null);
    }
    for (unsigned i = 0; i < 10; i++)
    {
	mallocator_dereference(child[i]);
    }
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

TestSuite *mallocator_tests(void)
{
    TestSuite *suite = create_test_suite();
    add_test_with_context(suite, mallocator, can_be_referenced);
    add_test_with_context(suite, mallocator, has_a_name);
    add_test_with_context(suite, mallocator, has_a_full_name);
    add_test_with_context(suite, mallocator, has_no_family);
    add_test_with_context(suite, mallocator, can_create_children);
    add_test_with_context(suite, mallocator, can_malloc);
    add_test_with_context(suite, mallocator, can_calloc);
    add_test_with_context(suite, mallocator, can_realloc);
    return suite;
}
