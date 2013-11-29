#include <cgreen/cgreen.h>

#include "mallocator_tracer.h"
#include "mallocator.h"

#include <malloc.h>

static struct mallinfo mallinfo_before;

Describe(mallocator_tracer);

BeforeEach(mallocator_tracer) 
{
    mallinfo_before = mallinfo();
}

AfterEach(mallocator_tracer)
{
    /* Ensure that there are no memory leaks */
    struct mallinfo mallinfo_after = mallinfo();
    assert_that(mallinfo_after.uordblks, is_equal_to(mallinfo_before.uordblks));
    assert_that(mallinfo_after.fordblks, is_equal_to(mallinfo_before.fordblks));
}

static void trace_copy(void *arg, const mallocator_tracer_event_t *event)
{
    mallocator_tracer_event_t *p_event = arg;
    *p_event = *event;
}

Ensure(mallocator_tracer, can_be_created)
{
    mallocator_t *m = mallocator_tracer_create("test", trace_copy, NULL);
    assert_that(m, is_non_null);
    mallocator_dereference(m);
}

Ensure(mallocator_tracer, has_a_name)
{
    mallocator_t *m = mallocator_tracer_create("test", trace_copy, NULL);
    const char *name = mallocator_name(m);
    assert_that(name, is_equal_to_string("test"));
    mallocator_dereference(m);
}

Ensure(mallocator_tracer, traces_malloc)
{
    mallocator_tracer_event_t event = { 0 };
    mallocator_t *m = mallocator_tracer_create("test", trace_copy, &event);
    unsigned num = 1024;
    int *ints = mallocator_malloc(m, num * sizeof(int));
    assert_that(ints, is_non_null);
    for (unsigned i = 0; i < num; i++)
    {
	ints[i] = i;
    }
    assert_that(event.type, is_equal_to(MALLOCATOR_TRACER_MALLOC));
    assert_that(event.ptr, is_equal_to(ints));
    assert_that(event.e.malloc.size, is_equal_to(num * sizeof(int)));
    assert_that(event.backtrace_len, is_greater_than(0));
    for (unsigned i = 0; i < event.backtrace_len; i++)
    {
	assert_that(event.backtrace[i], is_non_null);
    }
    mallocator_free(m, ints, num * sizeof(int));
    assert_that(event.type, is_equal_to(MALLOCATOR_TRACER_FREE));
    assert_that(event.ptr, is_equal_to(ints));
    assert_that(event.e.free.size, is_equal_to(num * sizeof(int)));
    assert_that(event.backtrace_len, is_greater_than(0));
    for (unsigned i = 0; i < event.backtrace_len; i++)
    {
	assert_that(event.backtrace[i], is_non_null);
    }
    mallocator_dereference(m);
}

Ensure(mallocator_tracer, traces_calloc)
{
    mallocator_tracer_event_t event = { 0 };
    mallocator_t *m = mallocator_tracer_create("test", trace_copy, &event);
    unsigned num = 1024;
    int *ints = mallocator_calloc(m, num, sizeof(int));
    assert_that(ints, is_non_null);
    for (unsigned i = 0; i < num; i++)
    {
	assert_that(ints[i], is_equal_to(0));
	ints[i] = i;
    }
    assert_that(event.type, is_equal_to(MALLOCATOR_TRACER_CALLOC));
    assert_that(event.ptr, is_equal_to(ints));
    assert_that(event.e.calloc.nmemb, is_equal_to(num));
    assert_that(event.e.calloc.size, is_equal_to(sizeof(int)));
    assert_that(event.backtrace_len, is_greater_than(0));
    for (unsigned i = 0; i < event.backtrace_len; i++)
    {
	assert_that(event.backtrace[i], is_non_null);
    }
    mallocator_free(m, ints, num * sizeof(int));
    assert_that(event.type, is_equal_to(MALLOCATOR_TRACER_FREE));
    assert_that(event.ptr, is_equal_to(ints));
    assert_that(event.e.free.size, is_equal_to(num * sizeof(int)));
    assert_that(event.backtrace_len, is_greater_than(0));
    for (unsigned i = 0; i < event.backtrace_len; i++)
    {
	assert_that(event.backtrace[i], is_non_null);
    }
    mallocator_dereference(m);
}

Ensure(mallocator_tracer, traces_realloc)
{
    mallocator_tracer_event_t event = { 0 };
    mallocator_t *m = mallocator_tracer_create("test", trace_copy, &event);
    unsigned num = 1024;
    int *ints = mallocator_realloc(m, NULL, 0, num * sizeof(int));
    assert_that(ints, is_non_null);
    for (unsigned i = 0; i < num; i++)
    {
	ints[i] = i;
    }
    assert_that(event.type, is_equal_to(MALLOCATOR_TRACER_REALLOC));
    assert_that(event.ptr, is_equal_to(ints));
    assert_that(event.e.realloc.old_ptr, is_null);
    assert_that(event.e.realloc.old_size, is_equal_to(0));
    assert_that(event.e.realloc.new_size, is_equal_to(num * sizeof(int)));
    assert_that(event.backtrace_len, is_greater_than(0));
    for (unsigned i = 0; i < event.backtrace_len; i++)
    {
	assert_that(event.backtrace[i], is_non_null);
    }
    unsigned more_num = 10 * num;
    int *more_ints = mallocator_realloc(m, ints, num * sizeof(int), more_num * sizeof(int));
    assert_that(more_ints, is_non_null);
    for (unsigned i = 0; i < more_num; i++)
    {
	if (i < num) assert_that(ints[i], is_equal_to(i));
	else ints[i] = i;
    }
    assert_that(event.type, is_equal_to(MALLOCATOR_TRACER_REALLOC));
    assert_that(event.ptr, is_equal_to(ints));
    assert_that(event.e.realloc.old_ptr, is_equal_to(ints));
    assert_that(event.e.realloc.old_size, is_equal_to(num * sizeof(int)));
    assert_that(event.e.realloc.new_size, is_equal_to(more_num * sizeof(int)));
    assert_that(event.backtrace_len, is_greater_than(0));
    for (unsigned i = 0; i < event.backtrace_len; i++)
    {
	assert_that(event.backtrace[i], is_non_null);
    }
    int *no_ints = mallocator_realloc(m, more_ints, more_num * sizeof(int), 0);
    assert_that(no_ints, is_null);
    assert_that(event.type, is_equal_to(MALLOCATOR_TRACER_REALLOC));
    assert_that(event.ptr, is_null);
    assert_that(event.e.realloc.old_ptr, is_equal_to(more_ints));
    assert_that(event.e.realloc.old_size, is_equal_to(more_num * sizeof(int)));
    assert_that(event.e.realloc.new_size, is_equal_to(0));
    assert_that(event.backtrace_len, is_greater_than(0));
    for (unsigned i = 0; i < event.backtrace_len; i++)
    {
	assert_that(event.backtrace[i], is_non_null);
    }
    mallocator_dereference(m);
}

TestSuite *mallocator_tracer_tests(void)
{
    TestSuite *suite = create_test_suite();
    add_test_with_context(suite, mallocator_tracer, can_be_created);
    add_test_with_context(suite, mallocator_tracer, has_a_name);
    add_test_with_context(suite, mallocator_tracer, traces_malloc);
    add_test_with_context(suite, mallocator_tracer, traces_calloc);
    add_test_with_context(suite, mallocator_tracer, traces_realloc);
    return suite;
}
