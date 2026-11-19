#ifndef STATDATA_TESTS_H
#define STATDATA_TESTS_H

#include <stddef.h>

#include "statdata.h"

typedef struct
{
	const StatData *a;
	const size_t a_size;
	const StatData *b;
	const size_t b_size;
	const StatData *ref;
	const size_t ref_size;
} TestData;

/* the list of all test cases, defined in tests.c */
extern const TestData *GetTests(size_t *count);

#endif /* STATDATA_TESTS_H */
