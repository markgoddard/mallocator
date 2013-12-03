#include <cgreen/cgreen.h>

#include "mallocator_monkey.h"
#include "mallocator.h"

#include <malloc.h>

static struct mallinfo mallinfo_before;

Describe(mallocator_monkey);

BeforeEach(mallocator_monkey) 
{
    mallinfo_before = mallinfo();
}

AfterEach(mallocator_monkey)
{
    /* Ensure that there are no memory leaks */
    struct mallinfo mallinfo_after = mallinfo();
    assert_that(mallinfo_after.uordblks, is_equal_to(mallinfo_before.uordblks));
    assert_that(mallinfo_after.fordblks, is_equal_to(mallinfo_before.fordblks));
}

static bool never_fail(void *arg)
{
    return false;
}

static bool always_fail(void *arg)
{
    return true;
}

Ensure(mallocator_monkey, can_be_created)
{
    mallocator_t *m = mallocator_monkey_create_custom("test", never_fail, NULL);
    assert_that(m, is_non_null);
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, has_a_name)
{
    mallocator_t *m = mallocator_monkey_create_custom("test", never_fail, NULL);
    const char *name = mallocator_name(m);
    assert_that(name, is_equal_to_string("test"));
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_malloc)
{
    mallocator_t *m = mallocator_monkey_create_custom("test", never_fail, NULL);
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
    mallocator_t *m = mallocator_monkey_create_custom("test", never_fail, NULL);
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
    mallocator_t *m = mallocator_monkey_create_custom("test", never_fail, NULL);
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

Ensure(mallocator_monkey, can_fail_malloc)
{
    mallocator_t *m = mallocator_monkey_create_custom("test", always_fail, NULL);
    unsigned num = 1024;
    int *ints = mallocator_malloc(m, num * sizeof(int));
    assert_that(ints, is_null);
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_fail_calloc)
{
    mallocator_t *m = mallocator_monkey_create_custom("test", always_fail, NULL);
    unsigned num = 1024;
    int *ints = mallocator_calloc(m, num, sizeof(int));
    assert_that(ints, is_null);
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_fail_realloc)
{
    mallocator_t *m = mallocator_monkey_create_custom("test", always_fail, NULL);
    unsigned num = 1024;
    int *ints = mallocator_realloc(m, NULL, 0, num * sizeof(int));
    assert_that(ints, is_null);
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_step_malloc)
{
    const unsigned num_success = 3, num_failure = 2;
    mallocator_t *m = mallocator_monkey_create_step("test", num_success, num_failure, false);
    unsigned num = 1024;
    for (unsigned i = 0; i < num_success; i++)
    {
	int *ints = mallocator_malloc(m, num * sizeof(int));
	assert_that(ints, is_non_null);
	mallocator_free(m, ints, num * sizeof(int));
    }
    for (unsigned i = 0; i < num_failure; i++)
    {
	int *ints = mallocator_malloc(m, num * sizeof(int));
	assert_that(ints, is_null);
    }
    for (unsigned i = 0; i < num_success * 2; i++)
    {
	int *ints = mallocator_malloc(m, num * sizeof(int));
	assert_that(ints, is_non_null);
	mallocator_free(m, ints, num * sizeof(int));
    }
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_step_calloc)
{
    const unsigned num_success = 2, num_failure = 3;
    mallocator_t *m = mallocator_monkey_create_step("test", num_success, num_failure, false);
    unsigned num = 1024;
    for (unsigned i = 0; i < num_success; i++)
    {
	int *ints = mallocator_calloc(m, num, sizeof(int));
	assert_that(ints, is_non_null);
	mallocator_free(m, ints, num * sizeof(int));
    }
    for (unsigned i = 0; i < num_failure; i++)
    {
	int *ints = mallocator_calloc(m, num, sizeof(int));
	assert_that(ints, is_null);
    }
    for (unsigned i = 0; i < num_success * 2; i++)
    {
	int *ints = mallocator_calloc(m, num, sizeof(int));
	assert_that(ints, is_non_null);
	mallocator_free(m, ints, num * sizeof(int));
    }
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_step_realloc)
{
    const unsigned num_success = 4, num_failure = 4;
    mallocator_t *m = mallocator_monkey_create_step("test", num_success, num_failure, false);
    unsigned num = 1024;
    int *ints = NULL;
    for (unsigned i = 0; i < num_success; i++)
    {
	ints = mallocator_realloc(m, ints, i * num * sizeof(int), (i + 1) * num * sizeof(int));
	assert_that(ints, is_non_null);
    }
    for (unsigned i = 0; i < num_failure; i++)
    {
	int *ints2 = mallocator_realloc(m, ints, num_success * num * sizeof(int), (num_success + 1) * num * sizeof(int));
	assert_that(ints2, is_null);
    }
    for (unsigned i = 0; i < num_success * 2; i++)
    {
	ints = mallocator_realloc(m, ints, (i + num_success) * num * sizeof(int), (i + num_success + 1) * num * sizeof(int));
	assert_that(ints, is_non_null);
    }
    ints = mallocator_realloc(m, ints, num_success * 2 * num * sizeof(int), 0);
    assert_that(ints, is_null);
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_step_repeat_malloc)
{
    const unsigned num_success = 3, num_failure = 2;
    mallocator_t *m = mallocator_monkey_create_step("test", num_success, num_failure, true);
    unsigned num = 1024;
    for (unsigned repeat = 0; repeat < 3; repeat++)
    {
	for (unsigned i = 0; i < num_success; i++)
	{
	    int *ints = mallocator_malloc(m, num * sizeof(int));
	    assert_that(ints, is_non_null);
	    mallocator_free(m, ints, num * sizeof(int));
	}
	for (unsigned i = 0; i < num_failure; i++)
	{
	    int *ints = mallocator_malloc(m, num * sizeof(int));
	    assert_that(ints, is_null);
	}
    }
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_step_repeat_calloc)
{
    const unsigned num_success = 2, num_failure = 3;
    mallocator_t *m = mallocator_monkey_create_step("test", num_success, num_failure, true);
    unsigned num = 1024;
    for (unsigned repeat = 0; repeat < 3; repeat++)
    {
	for (unsigned i = 0; i < num_success; i++)
	{
	    int *ints = mallocator_calloc(m, num, sizeof(int));
	    assert_that(ints, is_non_null);
	    mallocator_free(m, ints, num * sizeof(int));
	}
	for (unsigned i = 0; i < num_failure; i++)
	{
	    int *ints = mallocator_calloc(m, num, sizeof(int));
	    assert_that(ints, is_null);
	}
    }
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_step_repeat_realloc)
{
    const unsigned num_success = 4, num_failure = 4;
    mallocator_t *m = mallocator_monkey_create_step("test", num_success, num_failure, true);
    unsigned num = 1024;
    int *ints = NULL;
    for (unsigned repeat = 0; repeat < 3; repeat++)
    {
	for (unsigned i = 0; i < num_success; i++)
	{
	    ints = mallocator_realloc(m, ints, i * num * sizeof(int), (i + 1) * num * sizeof(int));
	    assert_that(ints, is_non_null);
	}
	for (unsigned i = 0; i < num_failure; i++)
	{
	    int *ints2 = mallocator_realloc(m, ints, num_success * num * sizeof(int), (num_success + 1) * num * sizeof(int));
	    assert_that(ints2, is_null);
	}
	ints = mallocator_realloc(m, ints, num_success * 2 * num * sizeof(int), 0);
	assert_that(ints, is_null);
    }
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_random_malloc)
{
    const float p_fail = 0.1, p_success = 0.1;
    mallocator_t *m = mallocator_monkey_create_random("test", p_fail, p_success);
    unsigned num_success = 0;
    unsigned num = 1024;
    unsigned total = 1000;
    for (unsigned i = 0; i < total; i++)
    {
	int *ints = mallocator_malloc(m, num * sizeof(int));
	if (ints)
	{
	    num_success++;
	    mallocator_free(m, ints, num * sizeof(int));
	}
    }
    assert_that(num_success, is_less_than((0.6 * total)));
    assert_that(num_success, is_greater_than((0.4 * total)));
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_random_calloc)
{
    const float p_fail = 0.1, p_success = 0.2;
    mallocator_t *m = mallocator_monkey_create_random("test", p_fail, p_success);
    unsigned num_success = 0;
    unsigned num = 1024;
    unsigned total = 1000;
    for (unsigned i = 0; i < total; i++)
    {
	int *ints = mallocator_calloc(m, num, sizeof(int));
	if (ints)
	{
	    num_success++;
	    mallocator_free(m, ints, num * sizeof(int));
	}
    }
    assert_that(num_success, is_less_than((0.76 * total)));
    assert_that(num_success, is_greater_than((0.56 * total)));
    mallocator_dereference(m);
}

Ensure(mallocator_monkey, can_random_realloc)
{
    const float p_fail = 0.3, p_success = 0.1;
    mallocator_t *m = mallocator_monkey_create_random("test", p_fail, p_success);
    unsigned num_success = 0;
    unsigned num = 1024;
    unsigned total = 1000;
    for (unsigned i = 0; i < total; i++)
    {
	int *ints = mallocator_malloc(m, num * sizeof(int));
	if (ints)
	{
	    num_success++;
	    mallocator_free(m, ints, num * sizeof(int));
	}
    }
    assert_that(num_success, is_less_than((0.35 * total)));
    assert_that(num_success, is_greater_than((0.15 * total)));
    mallocator_dereference(m);
}

TestSuite *mallocator_monkey_tests(void)
{
    TestSuite *suite = create_test_suite();
    add_test_with_context(suite, mallocator_monkey, can_be_created);
    add_test_with_context(suite, mallocator_monkey, has_a_name);
    add_test_with_context(suite, mallocator_monkey, can_malloc);
    add_test_with_context(suite, mallocator_monkey, can_calloc);
    add_test_with_context(suite, mallocator_monkey, can_realloc);
    add_test_with_context(suite, mallocator_monkey, can_fail_malloc);
    add_test_with_context(suite, mallocator_monkey, can_fail_calloc);
    add_test_with_context(suite, mallocator_monkey, can_fail_realloc);
    add_test_with_context(suite, mallocator_monkey, can_step_malloc);
    add_test_with_context(suite, mallocator_monkey, can_step_calloc);
    add_test_with_context(suite, mallocator_monkey, can_step_realloc);
    add_test_with_context(suite, mallocator_monkey, can_step_repeat_malloc);
    add_test_with_context(suite, mallocator_monkey, can_step_repeat_calloc);
    add_test_with_context(suite, mallocator_monkey, can_step_repeat_realloc);
    add_test_with_context(suite, mallocator_monkey, can_random_malloc);
    add_test_with_context(suite, mallocator_monkey, can_random_calloc);
    add_test_with_context(suite, mallocator_monkey, can_random_realloc);
    return suite;
}
