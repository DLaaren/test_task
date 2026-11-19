#include <assert.h>

#include "tests.h"

#define STATDATA_ARRAY_LEN(x) (sizeof(x) / sizeof(StatData))
_Static_assert(sizeof(StatData) > sizeof(void *),
			   "if sizeof(StatData) >= sizeof(NULL) = sizeof(void *), "
			   "then we may get size = 1 for an empty list");

#define TEST_ENTRY_CAT_(CASE, SUFFIX) CASE##SUFFIX

#define TEST_ENTRY(CASE) \
	{.a = TEST_ENTRY_CAT_(CASE, _a), \
	 .a_size = STATDATA_ARRAY_LEN(TEST_ENTRY_CAT_(CASE, _a)), \
	 .b = TEST_ENTRY_CAT_(CASE, _b), \
	 .b_size = STATDATA_ARRAY_LEN(TEST_ENTRY_CAT_(CASE, _b)), \
	 .ref = TEST_ENTRY_CAT_(CASE, _reference), \
	 .ref_size = STATDATA_ARRAY_LEN(TEST_ENTRY_CAT_(CASE, _reference))}

/* -------- Case 1 -------- */
static const StatData case_1_a[2] =
{{.id = 90889, .count = 13, .cost = 3.567,  .primary = 0, .mode = 3},
 {.id = 90089, .count = 1,  .cost = 88.90,  .primary = 1, .mode = 0}};

static const StatData case_1_b[2] =
{{.id = 90089, .count = 13,   .cost = 0.011,   .primary = 0, .mode = 2},
 {.id = 90189, .count = 1000, .cost = 1.00003, .primary = 1, .mode = 2}};

static const StatData case_1_reference[3] =
{{.id = 90189, .count = 1000, .cost = 1.00003, .primary = 1, .mode = 2},
 {.id = 90889, .count = 13,   .cost = 3.567,   .primary = 0, .mode = 3},
 {.id = 90089, .count = 14,   .cost = 88.911,  .primary = 0, .mode = 2}};

/* -------- Case 2: no overlap -------- */
static const StatData case_2_a[2] =
{{.id = 100, .count = 5, .cost = 1.0, .primary = 0, .mode = 0},
 {.id = 50,  .count = 3, .cost = 2.0, .primary = 1, .mode = 1}};

static const StatData case_2_b[2] =
{{.id = 80, .count = 7, .cost = 3.0, .primary = 0, .mode = 2},
 {.id = 30, .count = 9, .cost = 4.0, .primary = 1, .mode = 3}};

static const StatData case_2_reference[4] =
{{.id = 100, .count = 5, .cost = 1.0, .primary = 0, .mode = 0},
 {.id = 50,  .count = 3, .cost = 2.0, .primary = 1, .mode = 1},
 {.id = 80,  .count = 7, .cost = 3.0, .primary = 0, .mode = 2},
 {.id = 30,  .count = 9, .cost = 4.0, .primary = 1, .mode = 3}};

/* -------- Case 3: full overlap -------- */
static const StatData case_3_a[2] =
{{.id = 100, .count = 5, .cost = 1.5, .primary = 0, .mode = 0},
 {.id = 50,  .count = 3, .cost = 2.5, .primary = 1, .mode = 1}};

static const StatData case_3_b[2] =
{{.id = 100, .count = 2, .cost = 3.5, .primary = 1, .mode = 2},
 {.id = 50,  .count = 7, .cost = 4.5, .primary = 0, .mode = 3}};

static const StatData case_3_reference[2] =
{{.id = 100, .count = 7,  .cost = 5.0, .primary = 0, .mode = 2},
 {.id = 50,  .count = 10, .cost = 7.0, .primary = 0, .mode = 3}};

/* -------- Case 4: empty a -------- */
static const StatData *const case_4_a = NULL;

static const StatData case_4_b[2] =
{{.id = 10, .count = 1, .cost = 0.5, .primary = 0, .mode = 0},
 {.id = 20, .count = 2, .cost = 1.5, .primary = 1, .mode = 1}};

static const StatData case_4_reference[2] =
{{.id = 10, .count = 1, .cost = 0.5, .primary = 0, .mode = 0},
 {.id = 20, .count = 2, .cost = 1.5, .primary = 1, .mode = 1}};

/* -------- Case 5: empty b -------- */
static const StatData case_5_a[2] =
{{.id = 10, .count = 1, .cost = 0.5, .primary = 0, .mode = 0},
 {.id = 20, .count = 2, .cost = 1.5, .primary = 1, .mode = 1}};

static const StatData *const case_5_b = NULL;

static const StatData case_5_reference[2] =
{{.id = 10, .count = 1, .cost = 0.5, .primary = 0, .mode = 0},
 {.id = 20, .count = 2, .cost = 1.5, .primary = 1, .mode = 1}};

/* -------- Case 6: inputs ascending, output cost-ordered -------- */
static const StatData case_6_a[3] =
{{.id = 10, .count = 1, .cost = 0.5, .primary = 0, .mode = 0},
 {.id = 20, .count = 2, .cost = 1.5, .primary = 0, .mode = 1},
 {.id = 30, .count = 3, .cost = 2.5, .primary = 0, .mode = 2}};

static const StatData case_6_b[2] =
{{.id = 15, .count = 4, .cost = 3.5, .primary = 1, .mode = 0},
 {.id = 25, .count = 5, .cost = 4.5, .primary = 1, .mode = 1}};

static const StatData case_6_reference[5] =
{{.id = 10, .count = 1, .cost = 0.5, .primary = 0, .mode = 0},
 {.id = 20, .count = 2, .cost = 1.5, .primary = 0, .mode = 1},
 {.id = 30, .count = 3, .cost = 2.5, .primary = 0, .mode = 2},
 {.id = 15, .count = 4, .cost = 3.5, .primary = 1, .mode = 0},
 {.id = 25, .count = 5, .cost = 4.5, .primary = 1, .mode = 1}};

/* -------- Case 7: extreme values -------- */
static const StatData case_7_a[2] =
{{.id = 2147483647L,      .count = 1000000000, .cost = 1.5e10,  .primary = 0, .mode = 0},
 {.id = -2147483647L - 1, .count = 1000000000, .cost = 1.5e-10, .primary = 1, .mode = 3}};

static const StatData case_7_b[2] =
{{.id = 2147483647L, .count = 147483647, .cost = 1.5e10, .primary = 1, .mode = 1},
 {.id = 0L,          .count = 0,         .cost = 0.0,    .primary = 0, .mode = 0}};

static const StatData case_7_reference[3] =
{{.id = 0L,               .count = 0,          .cost = 0.0,     .primary = 0, .mode = 0},
 {.id = -2147483647L - 1, .count = 1000000000, .cost = 1.5e-10, .primary = 1, .mode = 3},
 {.id = 2147483647L,      .count = 1147483647, .cost = 3.0e10,  .primary = 0, .mode = 1}};

/* -------- Case 8: equal cost, tie-break by id -------- */
static const StatData case_8_a[2] =
{{.id = 200, .count = 1, .cost = 5.0, .primary = 0, .mode = 0},
 {.id = 100, .count = 1, .cost = 5.0, .primary = 0, .mode = 0}};

static const StatData case_8_b[1] =
{{.id = 300, .count = 1, .cost = 5.0, .primary = 1, .mode = 1}};

static const StatData case_8_reference[3] =
{{.id = 100, .count = 1, .cost = 5.0, .primary = 0, .mode = 0},
 {.id = 200, .count = 1, .cost = 5.0, .primary = 0, .mode = 0},
 {.id = 300, .count = 1, .cost = 5.0, .primary = 1, .mode = 1}};

/* -------- Case 9: partial overlap -------- */
static const StatData case_9_a[3] =
{{.id = 30, .count = 1, .cost = 1.0, .primary = 0, .mode = 0},
 {.id = 20, .count = 2, .cost = 2.0, .primary = 0, .mode = 1},
 {.id = 10, .count = 3, .cost = 3.0, .primary = 0, .mode = 2}};

static const StatData case_9_b[2] =
{{.id = 20, .count = 4, .cost = 4.0, .primary = 1, .mode = 3},
 {.id = 5,  .count = 5, .cost = 5.0, .primary = 1, .mode = 0}};

static const StatData case_9_reference[4] =
{{.id = 30, .count = 1, .cost = 1.0, .primary = 0, .mode = 0},
 {.id = 10, .count = 3, .cost = 3.0, .primary = 0, .mode = 2},
 {.id = 5,  .count = 5, .cost = 5.0, .primary = 1, .mode = 0},
 {.id = 20, .count = 6, .cost = 6.0, .primary = 0, .mode = 3}};

/* -------- Case 10: single matching element -------- */
static const StatData case_10_a[1] =
{{.id = 42, .count = 1, .cost = 1.0, .primary = 1, .mode = 3}};

static const StatData case_10_b[1] =
{{.id = 42, .count = 1, .cost = 1.0, .primary = 0, .mode = 0}};

static const StatData case_10_reference[1] =
{{.id = 42, .count = 2, .cost = 2.0, .primary = 0, .mode = 3}};

/* -------- Case 11: same id appears 3 times total -------- */
static const StatData case_11_a[3] =
{{.id = 7, .count = 1, .cost = 1.0, .primary = 1, .mode = 1},
 {.id = 7, .count = 2, .cost = 2.0, .primary = 0, .mode = 3},
 {.id = 5, .count = 3, .cost = 3.0, .primary = 1, .mode = 2}};

static const StatData case_11_b[2] =
{{.id = 7, .count = 4, .cost = 4.0, .primary = 1, .mode = 0},
 {.id = 9, .count = 5, .cost = 5.0, .primary = 0, .mode = 1}};

static const StatData case_11_reference[3] =
{{.id = 5, .count = 3, .cost = 3.0, .primary = 1, .mode = 2},
 {.id = 9, .count = 5, .cost = 5.0, .primary = 0, .mode = 1},
 {.id = 7, .count = 7, .cost = 7.0, .primary = 0, .mode = 3}};

/*
 * all test cases.
 * to add a new one: write case_N_a / case_N_b / case_N_reference above and
 * add one line TEST_ENTRY(case_N) here; cases with an empty side have no
 * array for it and stay explicit (TEST_ENTRY cannot use sizeof on NULL)
 */
static const TestData tests[] =
{
	TEST_ENTRY(case_1),
	TEST_ENTRY(case_2),
	TEST_ENTRY(case_3),
	TEST_ENTRY(case_4),
	TEST_ENTRY(case_5),
	TEST_ENTRY(case_6),
	TEST_ENTRY(case_7),
	TEST_ENTRY(case_8),
	TEST_ENTRY(case_9),
	TEST_ENTRY(case_10),
	TEST_ENTRY(case_11),
};

const TestData *
GetTests(size_t *count)
{
	assert(count);
	*count = sizeof(tests) / sizeof(TestData);

	return tests;
}
