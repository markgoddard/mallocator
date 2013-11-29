#ifndef MALLOCATOR_TRACER_H
#define MALLOCATOR_TRACER_H

#include "mallocator_impl.h"
#include <stdbool.h>

/*
 * mallocator implementation which traces all memory allocation and frees.
 */

enum { MALLOCATOR_TRACER_BACKTRACE_MAX = 8 };

typedef enum
{
    MALLOCATOR_TRACER_MALLOC,
    MALLOCATOR_TRACER_CALLOC,
    MALLOCATOR_TRACER_REALLOC,
    MALLOCATOR_TRACER_FREE,
} mallocator_tracer_type_t;

typedef struct
{
    mallocator_tracer_type_t type;
    void *ptr;
    union
    {
	struct
	{
	    size_t size;
	} malloc;
	struct
	{
	    size_t nmemb;
	    size_t size;
	} calloc;
	struct
	{
	    void *old_ptr;
	    size_t old_size;
	    size_t new_size;
	} realloc;
	struct
	{
	    size_t size;
	} free;
    } e;
    void *backtrace[MALLOCATOR_TRACER_BACKTRACE_MAX];
    size_t backtrace_len;
} mallocator_tracer_event_t;

typedef void (*mallocator_tracer_fn)(void *arg, const mallocator_tracer_event_t *event);

const char *mallocator_tracer_type_str(mallocator_tracer_type_t type);

mallocator_t *mallocator_tracer_create(const char *name, mallocator_tracer_fn fn, void *arg);

#endif // MALLOCATOR_TRACER_H
