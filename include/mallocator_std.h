#ifndef MALLOCATOR_STD_H
#define MALLOCATOR_STD_H

#include "mallocator.h"

/*
 * Simple mallocator implementation:
 * - Implements the mallocator interface
 * - Uses stdlib malloc/free/calloc/realloc
 * - No hierarchy
 */
typedef struct mallocator_std mallocator_std_t;

mallocator_std_t *mallocator_std_create(const char *name);

mallocator_t *mallocator_std_mallocator(mallocator_std_t *mallocator);

#endif // MALLOCATOR_STD_H
