#include <cgreen/cgreen.h>

#include "mallocator.h"

#include <pthread.h>
#include <malloc.h>
#include <stdio.h>

#define debugf(...) //printf(__VA_ARGS__)

Describe(mallocator_concurrency)

static struct mallinfo mallinfo_before;

static void *dummy_thread(void *arg) { return NULL; }

BeforeEach(mallocator_concurrency) 
{
    /* HACK! pthreads keeps a number of thread stacks allocated after a thread has terminated
     * to avoid reallocation when threads are created. This affects our memory low watermark,
     * so create a number of threads */
    for (unsigned num_dummy_threads = 8; ; num_dummy_threads *= 2)
    {
	mallinfo_before = mallinfo();
	pthread_t dummy_tid[num_dummy_threads];
	for (unsigned i = 0; i < num_dummy_threads; i++)
	    assert_that(pthread_create(&dummy_tid[i], NULL, dummy_thread, NULL), is_equal_to(0));
	for (unsigned i = 0; i < num_dummy_threads; i++)
	    assert_that(pthread_join(dummy_tid[i], NULL), is_equal_to(0));

	struct mallinfo mallinfo_after = mallinfo();
	if (mallinfo_after.uordblks == mallinfo_before.uordblks &&
	    mallinfo_after.fordblks == mallinfo_before.fordblks)
	    break;
    }
}

AfterEach(mallocator_concurrency)
{
    /* Ensure that there are no memory leaks */
    struct mallinfo mallinfo_after = mallinfo();
    assert_that(mallinfo_after.uordblks, is_equal_to(mallinfo_before.uordblks));
    assert_that(mallinfo_after.fordblks, is_equal_to(mallinfo_before.fordblks));
}

enum { max_mallocators = 64 };

typedef struct
{
    unsigned index;
    unsigned num_iterations;
    mallocator_t *root;
} test_data_t;

typedef struct
{
    mallocator_t *mallocator;
    unsigned ref_count;		/* References owned by this thread specifically */
} test_mallocator_t;

static void *thread(void *arg)
{
    test_data_t *data = arg;
    unsigned index = data->index;
    test_mallocator_t mallocators[max_mallocators] = { [0] = { .mallocator = data->root, .ref_count = 0 } };
    unsigned num_mallocators = 1;
    unsigned next_mallocator = 0;
    for (unsigned i = 0; i < data->num_iterations; i++)
    {
	unsigned m_index = rand() % num_mallocators;
	test_mallocator_t *tm = &mallocators[m_index];
	mallocator_t *mallocator = tm->mallocator;
	unsigned r = rand() % 100;
	if (r < 10 && num_mallocators < max_mallocators)
	{
	    debugf("[%x] (%s) create child %u\n", index, mallocator_name(mallocator), next_mallocator);
	    test_mallocator_t *new_tm = &mallocators[num_mallocators];
	    char name[16];
	    snprintf(name, sizeof(name), "t%u[%u]", index, next_mallocator++);
	    new_tm->mallocator = mallocator_create_child(mallocator, name);
	    assert_that(new_tm->mallocator, is_non_null);
	    new_tm->ref_count = 1;
	    num_mallocators++;
	}
	else if (r < 20 && m_index > 0)
	{
	    debugf("[%x] (%s) ref\n", index, mallocator_name(mallocator));
	    mallocator_reference(mallocator);
	    tm->ref_count++;
	}
	else if (r < 40 && m_index > 0)
	{
	    debugf("[%x] (%s) deref\n", index, mallocator_name(mallocator));
	    mallocator_dereference(mallocator);
	    if (--tm->ref_count == 0)
	    {
		*tm = mallocators[num_mallocators - 1];
		num_mallocators--;
	    }
	}
	else if (r < 50)
	{
	    debugf("[%x] (%s) stats\n", index, mallocator_name(mallocator));
	    mallocator_stats_t stats;
	    mallocator_stats(mallocator, &stats);
	}
	else if (r < 60)
	{
	    debugf("[%x] (%s) iterate\n", index, mallocator_name(mallocator));
	    mallocator_t *curr = mallocator_child_begin(mallocator);
	    while (curr) curr = mallocator_child_next(curr);
	}
	else if (r < 70)
	{
	    debugf("[%x] (%s) ref child\n", index, mallocator_name(mallocator));
	    mallocator_t *curr = mallocator_child_begin(mallocator);
	    while (curr)
	    {
		unsigned r2 = rand() % 100;
		debugf("[%x] (%s) ref child %u\n", index, mallocator_name(mallocator), r2);
		if (r2 < 50)
		{
		    curr = mallocator_child_next(curr);
		}
		else if (r2 < 75)
		{
		    /* Get the first child, drop the reference to the parent */
		    mallocator_t *parent = curr;
		    curr = mallocator_child_begin(parent);
		    mallocator_dereference(parent);
		}
		else
		{
		    /* Keep the reference */
		    test_mallocator_t *curr_tm = NULL;
		    for (unsigned i = 0; i < num_mallocators; i++)
		    {
			if (mallocators[i].mallocator == curr)
			{
			    curr_tm = &mallocators[i];
			    break;
			}
		    }
		    if (curr_tm)
		    {
			debugf("[%x] (%s) ref child %s\n", index, mallocator_name(mallocator), mallocator_name(curr));
			curr_tm->ref_count++;
		    }
		    else if (num_mallocators < max_mallocators)
		    {
			debugf("[%x] (%s) ref child %s\n", index, mallocator_name(mallocator), mallocator_name(curr));
			curr_tm = &mallocators[num_mallocators++];
			curr_tm->mallocator = curr;
			curr_tm->ref_count = 1;
		    }
		    else
		    {
			mallocator_dereference(curr);
		    }
		    break;
		}
	    }	
	}
	else if (r < 80)
	{
	    size_t size = (rand() % 1024) + 1;
	    debugf("[%x] (%s) malloc %zu\n", index, mallocator_name(mallocator), size);
	    void *ptr = mallocator_malloc(mallocator, size);
	    assert_that(ptr, is_non_null);
	    mallocator_free(mallocator, ptr, size);
	}
	else if (r < 90)
	{
	    size_t nmemb = (rand() % 1024) + 1;
	    size_t size = (rand() % 1024) + 1;
	    debugf("[%x] (%s) calloc %zu x %zu\n", index, mallocator_name(mallocator), nmemb, size);
	    void *ptr = mallocator_calloc(mallocator, nmemb, size);
	    assert_that(ptr, is_non_null);
	    mallocator_free(mallocator, ptr, nmemb * size);
	}
	else
	{
	    size_t size = (rand() % 1024) + 1;
	    size_t new_size = (rand() % 1024) + 1;
	    debugf("[%x] (%s) realloc %zu -> %zu\n", index, mallocator_name(mallocator), size, new_size);
	    void *ptr = mallocator_realloc(mallocator, NULL, 0, size);
	    assert_that(ptr, is_non_null);
	    void *new_ptr = mallocator_realloc(mallocator, ptr, size, new_size);
	    assert_that(new_ptr, is_non_null);
	    mallocator_realloc(mallocator, new_ptr, new_size, 0);
	}
    }

    /* Clean up */
    for (unsigned i = 1; i < num_mallocators; i++)
    {
	test_mallocator_t *tm = &mallocators[i];
	mallocator_t *mallocator = tm->mallocator;

	/* Release all references */
	for ( ; tm->ref_count > 0; tm->ref_count--)
	    mallocator_dereference(mallocator);
    }
    return NULL;
}

Ensure(mallocator_concurrency, is_safe)
{
    unsigned num_iterations = 1000;
    unsigned num_threads = 4;
    test_data_t data[num_threads];
    pthread_t threads[num_threads];
    mallocator_t *root = mallocator_create("root");
    assert_that(root, is_non_null);
    for (unsigned i = 0; i < num_threads; i++)
    {
	data[i].index = i;
	data[i].num_iterations = num_iterations;
	data[i].root = root;
	assert_that(pthread_create(&threads[i], NULL, thread, &data[i]), is_equal_to(0));
    }
    for (unsigned i = 0; i < num_threads; i++)
    {
	assert_that(pthread_join(threads[i], NULL), is_equal_to(0));
    }
    assert_that(mallocator_child_begin(root), is_null);
    mallocator_dereference(root);
}

TestSuite *mallocator_concurrency_tests(void)
{
    TestSuite *suite = create_test_suite();
    add_test_with_context(suite, mallocator_concurrency, is_safe);
    return suite;
}
