#ifndef MALLOCATOR_MONKEY_H
#define MALLOCATOR_MONKEY_H

#include "mallocator_impl.h"
#include <stdbool.h>

/*
 * Unreliable mallocator implementation (name inspired by the Netflix Chaos monkey).
 * - Implements the mallocator interface
 * - Uses stdlib malloc/free/calloc/realloc
 * - No hierarchy
 * - Injects failures in a controlled manner
 * - Failures may be at random intervals or after a number of allocations
 */

typedef struct mallocator_monkey mallocator_monkey_t;

/* Custom failure function */
typedef bool (*mallocator_monkey_fail_fn)(void *arg);

mallocator_monkey_t *mallocator_monkey_create_random(float p_failure, float p_recovery);

mallocator_monkey_t *mallocator_monkey_create_step(unsigned num_success, unsigned num_failure, bool repeat);

mallocator_monkey_t *mallocator_monkey_create_custom(mallocator_monkey_fail_fn fn, void *arg);

mallocator_impl_t *mallocator_monkey_impl(mallocator_monkey_t *mallocator);

#endif // MALLOCATOR_MONKEY_H
