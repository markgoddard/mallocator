#include <cgreen/cgreen.h>

TestSuite *mallocator_std_tests(void);
TestSuite *mallocator_monkey_tests(void);

static TestSuite *mallocator_tests(void)
{
    TestSuite *suite = create_test_suite();
    add_suite(suite, mallocator_std_tests());
    add_suite(suite, mallocator_monkey_tests());
    return suite;
}

int main(int argc, char *argv[])
{
    return run_test_suite(mallocator_tests(), create_text_reporter());
}
