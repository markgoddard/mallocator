#include <cgreen/cgreen.h>

TestSuite *mallocator_tests(void);
TestSuite *mallocator_monkey_tests(void);
TestSuite *mallocator_tracer_tests(void);
TestSuite *mallocator_concurrency_tests(void);

static TestSuite *mallocator_all_tests(void)
{
    TestSuite *suite = create_test_suite();
    add_suite(suite, mallocator_tests());
    add_suite(suite, mallocator_monkey_tests());
    add_suite(suite, mallocator_tracer_tests());
    add_suite(suite, mallocator_concurrency_tests());
    return suite;
}

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
	return run_single_test(mallocator_all_tests(), argv[1], create_text_reporter());
    }
    return run_test_suite(mallocator_all_tests(), create_text_reporter());
}
