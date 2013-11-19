#include <cgreen/cgreen.h>

#include "mallocator_monkey.h"
#include "mallocator.h"

Describe(mallocator_monkey);
BeforeEach(mallocator_monkey) {}
AfterEach(mallocator_monkey) {}

static bool never_fail(void *arg)
{
    return false;
}

Ensure(mallocator_monkey, can_be_created)
{
    mallocator_monkey_t *std = mallocator_monkey_create_custom("test", never_fail, NULL);
    assert_that(std, is_non_null);
    mallocator_t *m = mallocator_monkey_mallocator(std);
    assert_that(m, is_non_null);
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, has_a_name)
{
    mallocator_t *m = mallocator_monkey_mallocator(mallocator_monkey_create_custom("test", never_fail, NULL));
    const char *name = mallocator_name(m);
    assert_that(name, is_equal_to_string("test"));
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_malloc)
{
    mallocator_t *m = mallocator_monkey_mallocator(mallocator_monkey_create_custom("test", never_fail, NULL));
    unsigned num = 1024;
    int *ints = mallocator_malloc(m, num * sizeof(int));
    assert_that(ints, is_non_null);
    for (unsigned i = 0; i < num; i++)
    {
	ints[i] = i;
    }
    mallocator_free(m, ints, num * sizeof(int));
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_calloc)
{
    mallocator_t *m = mallocator_monkey_mallocator(mallocator_monkey_create_custom("test", never_fail, NULL));
    unsigned num = 1024;
    int *ints = mallocator_calloc(m, num, sizeof(int));
    assert_that(ints, is_non_null);
    for (unsigned i = 0; i < num; i++)
    {
	assert_that(ints[i], is_equal_to(0));
	ints[i] = i;
    }
    mallocator_free(m, ints, num * sizeof(int));
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_realloc)
{
    mallocator_t *m = mallocator_monkey_mallocator(mallocator_monkey_create_custom("test", never_fail, NULL));
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
    mallocator_dereference(m);
}

TestSuite *mallocator_monkey_tests(void)
{
    TestSuite *suite = create_test_suite();
    add_test_with_context(suite, mallocator_monkey, can_be_created);
    add_test_with_context(suite, mallocator_monkey, has_a_name);
    return suite;
}
