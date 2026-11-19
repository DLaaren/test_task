#define _GNU_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "statdata.h"
#include "logging.h"

static inline void MergeStatdataEntries(StatData *dst, const StatData *src);
static int SortDumpByCostComparator(const void *a, const void *b);
static bool IsSortedById(const StatData *statdata, size_t statdata_size);
static int SortDumpByIdComparator(const void *a, const void *b);

int
StoreDump(const StatData *statdata, size_t statdata_size, const char *path)
{
	FILE *file = NULL;
	size_t written = 0;
	int retval = 0;

	STATDATA_LOG("trying to store statdata in \"%s\"", path);

	if (statdata_size == 0)
	{
		/* a non-NULL statdata is fine */
		STATDATA_LOG("statdata is empty");
	}
	else
	{
		assert(statdata != NULL);
	}

	if (path == NULL)
	{
		STATDATA_ERR("path is not specified");
		return -1;
	}

	if (access(path, F_OK) == 0)
	{
		STATDATA_ERR("file \"%s\" already exists: %m", path);
		return -1;
	}

	if ((file = fopen(path, "wb")) == NULL)
	{
		STATDATA_ERR("could not open \"%s\": %m", path);
		return -1;
	}

	/* write size of StatData array */
	written = fwrite(&statdata_size, sizeof(statdata_size), 1, file);
	if (written != 1 || ferror(file))
	{
		STATDATA_ERR("could not write size header to \"%s\": %m", path);

		retval = -1;
		goto cleanup;
	}

	/*
	 * Note: there are padding bytes in the StatData struct, which should be
	 * zero'd to keep all nice and neat, but since there is no other consumers
	 * except LoadDump, which simply ignores the values of padding bytes, I left
	 * them as they are
	 * One more note: if zero size, the fwrite writes nothing here
	 */
	written = fwrite(statdata, sizeof(StatData), statdata_size, file);
	if (written != statdata_size || ferror(file))
	{
		STATDATA_ERR("could not write statdata to \"%s\": %m", path);

		retval = -1;
		goto cleanup;
	}

cleanup:
	if (file && fclose(file) != 0)
	{
		STATDATA_ERR("could not close \"%s\": %m", path);
		retval = -1;
	}

	if (retval != 0 && file != NULL)
	{
		/* do not leave an underwritten file behind */
		if (unlink(path) != 0)
			STATDATA_ERR("could not remove truncated file \"%s\": %m", path);
	}
	else
	{
		STATDATA_LOG("statdata is successfully stored in \"%s\"", path);
	}

	return retval;
}

int
LoadDump(StatData **statdata, size_t *statdata_size, const char *path)
{
	FILE *file = NULL;
	size_t read = 0;
	int retval = 0;

	STATDATA_LOG("trying to load statdata from \"%s\"", path);

	if (path == NULL)
	{
		STATDATA_ERR("path is not specified");
		return -1;
	}

	/* init values */
	*statdata = NULL;
	*statdata_size = 0;

	if ((file = fopen(path, "rb")) == NULL)
	{
		STATDATA_ERR("could not open \"%s\": %m", path);
		return -1;
	}

	read = fread(statdata_size, sizeof(*statdata_size), 1, file);
	if (read != 1 || ferror(file))
	{
		STATDATA_ERR("could not read size header from \"%s\": %m", path);

		retval = -1;
		goto cleanup;
	}

	/*
	 * a dump is trusted only within the documented range: anything larger
	 * is a corrupted (or hostile) file, and the subsequent
	 * malloc(size * sizeof(StatData)) must not overflow size_t
	 */
	if (*statdata_size > MAX_STATDATA_ARR_SIZE)
	{
		STATDATA_ERR("stored size %zu is out of range [0, %d]",
		              *statdata_size, MAX_STATDATA_ARR_SIZE);

		retval = -1;
		goto cleanup;
	}

	if (*statdata_size == 0)
	{
		STATDATA_LOG("statdata is empty -- nothing to load");
	}
	else
	{
		*statdata = malloc(*statdata_size * sizeof(StatData));
		if (*statdata == NULL)
		{
			STATDATA_ERR("could not allocate memory: %m");

			retval = -1;
			goto cleanup;
		}

		read = fread(*statdata, sizeof(StatData), *statdata_size, file);
		if (read != *statdata_size || ferror(file))
		{
			STATDATA_ERR("could not read from \"%s\": %m", path);

			retval = -1;
			goto cleanup;
		}
	}

cleanup:
	if (file && fclose(file) != 0)
	{
		STATDATA_ERR("could not close \"%s\": %m", path);
		retval = -1;
	}

	if (retval != 0)
	{
		if (*statdata)
			free(*statdata);

		*statdata = NULL;
		*statdata_size = 0;
	}
	else
	{
		STATDATA_LOG("statdata is successfully loaded from \"%s\"", path);
	}

	return retval;
}

/*
 * array within [0, 100'000] elements --> max ~ 2.4 MB
 *
 * we may create hashmap to store arrays there and use to find unique
 * entries, but, it looks kinda strange like why not to store statdata
 * in hashmap generally?? secondly, i'd like to spend some time to come up
 * with idea of algorithm within arrays =)
 * ---
 * sort both arrays by id (an unsorted one via a scratch copy, so inputs
 * are not mutated) -- in a general case it would cost us
 * 200'000 * STATDATA_LOG 200'000 ~= 3'400'000 comparisons ~= 10-35 ms;
 * before sorting check if arrays are already id-sorted to skip the qsort;
 * then a single linear pass merges both arrays and collapses all runs of
 * equal ids. total cost in the worst scenario:
 *   200'000 (sort checking) + 200'000 * STATDATA_LOG 200'000 (sorting)
 *   + 200'000 (merging) ~= 3'800'000 ~= 15-40 ms <-- looks fine to me
 */

int
JoinDump(const StatData *statdata_a, size_t statdata_a_size,
		 const StatData *statdata_b, size_t statdata_b_size,
		 StatData **statdata_merged, size_t *statdata_merged_size)
{
	size_t i = 0, j = 0, m = 0;
	StatData *copy_a = NULL, *copy_b = NULL;
	const StatData *sorted_a = statdata_a, *sorted_b = statdata_b;
	StatData *statdata_acc = NULL;
	StatData *realloced_statdata_merged = NULL;
	int retval = 0;

	assert(statdata_merged != NULL);

	STATDATA_LOG("trying to join two array of statdata");

	*statdata_merged = NULL;
	*statdata_merged_size = 0;

	if (statdata_a_size == 0 && statdata_b_size == 0)
	{
		STATDATA_LOG("both arrays are empty");
		return 0;
	}

	/*
	 * inputs are not mutated: an unsorted array is copied into a scratch
	 * buffer and sorted there, an already id-sorted one is merged from
	 * the caller's memory as is
	 */
	if (statdata_a_size != 0)
	{
		assert(statdata_a != NULL);

		if (IsSortedById(statdata_a, statdata_a_size) == false)
		{
			copy_a = malloc(statdata_a_size * sizeof(StatData));
			if (copy_a == NULL)
			{
				STATDATA_ERR("could not allocate memory: %m");

				retval = -1;
				goto cleanup;
			}

			memcpy(copy_a, statdata_a, statdata_a_size * sizeof(StatData));
			qsort(copy_a, statdata_a_size, sizeof(StatData), SortDumpByIdComparator);
			sorted_a = copy_a;

			STATDATA_LOG("the first array has been sorted");
		}
	}

	if (statdata_b_size != 0)
	{
		assert(statdata_b != NULL);

		if (IsSortedById(statdata_b, statdata_b_size) == false)
		{
			copy_b = malloc(statdata_b_size * sizeof(StatData));
			if (copy_b == NULL)
			{
				STATDATA_ERR("could not allocate memory: %m");

				retval = -1;
				goto cleanup;
			}

			memcpy(copy_b, statdata_b, statdata_b_size * sizeof(StatData));
			qsort(copy_b, statdata_b_size, sizeof(StatData), SortDumpByIdComparator);
			sorted_b = copy_b;

			STATDATA_LOG("the second array has been sorted");
		}
	}

	*statdata_merged_size = statdata_a_size + statdata_b_size;
	*statdata_merged = calloc(*statdata_merged_size, sizeof(StatData));
	if (*statdata_merged == NULL)
	{
		STATDATA_ERR("could not allocate memory: %m");

		retval = -1;
		goto cleanup;
	}

	statdata_acc = *statdata_merged;

	/* give the accumulator the initial value; */
	if (statdata_a_size == 0)
	{
		*statdata_acc = sorted_b[0];
		j = 1;
	}
	else if (statdata_b_size == 0)
	{
		*statdata_acc = sorted_a[0];
		i = 1;
	}
	else if (sorted_a[0].id < sorted_b[0].id)
	{
		*statdata_acc = sorted_a[0];
		i = 1;
	}
	else /* sorted_b[0].id <= sorted_a[0].id */
	{
		*statdata_acc = sorted_b[0];
		j = 1;
	}
	m += 1;

	/*
	 * the loop below is skipped when one of the arrays is empty or the
	 * seed has consumed the shorter one -- the tails merge the rest the
	 * same way
	 */
	while (i < statdata_a_size && j < statdata_b_size)
	{
		if (sorted_a[i].id < sorted_b[j].id)
		{
			if (sorted_a[i].id != statdata_acc->id)
			{
				assert(sorted_a[i].id > statdata_acc->id);

				/* new id */
				m += 1;
				statdata_acc += 1;
				*statdata_acc = sorted_a[i];
			}
			else
			{
				MergeStatdataEntries(statdata_acc, sorted_a + i);
			}
			i += 1;
		}
		else if (sorted_b[j].id < sorted_a[i].id)
		{
			if (sorted_b[j].id != statdata_acc->id)
			{
				assert(sorted_b[j].id > statdata_acc->id);

				/* new id */
				m += 1;
				statdata_acc += 1;
				*statdata_acc = sorted_b[j];
			}
			else
			{
				MergeStatdataEntries(statdata_acc, sorted_b + j);
			}
			j += 1;
		}
		else
		{
			assert(sorted_a[i].id == sorted_b[j].id);

			if (sorted_a[i].id != statdata_acc->id)
			{
				/* new id */
				m += 1;
				statdata_acc += 1;
				*statdata_acc = sorted_a[i];
				MergeStatdataEntries(statdata_acc, sorted_b + j);
			}
			else
			{
				MergeStatdataEntries(statdata_acc, sorted_a + i);
				MergeStatdataEntries(statdata_acc, sorted_b + j);
			}
			i += 1; j += 1;
		}
	}

	/* check if at least one array is being "walked" up to its end */
	assert(i == statdata_a_size || j == statdata_b_size);

	/* merge the rest; we can't simply memcpy because there might be duplicates */
	while (i < statdata_a_size)
	{
		if (sorted_a[i].id != statdata_acc->id)
		{
			assert(sorted_a[i].id > statdata_acc->id);

			m += 1;
			statdata_acc += 1;
			*statdata_acc = sorted_a[i];
		}
		else
		{
			MergeStatdataEntries(statdata_acc, sorted_a + i);
		}
		i += 1;
	}

	while (j < statdata_b_size)
	{
		if (sorted_b[j].id != statdata_acc->id)
		{
			assert(sorted_b[j].id > statdata_acc->id);

			m += 1;
			statdata_acc += 1;
			*statdata_acc = sorted_b[j];
		}
		else
		{
			MergeStatdataEntries(statdata_acc, sorted_b + j);
		}
		j += 1;
	}

	assert(m <= *statdata_merged_size);

	/* truncate the allocated memory up to the actual size of elements */
	realloced_statdata_merged = realloc(*statdata_merged, m * sizeof(StatData));
	if (realloced_statdata_merged == NULL)
	{
		STATDATA_ERR("could not allocate memory: %m");

		retval = -1;
		goto cleanup;
	}

	*statdata_merged = realloced_statdata_merged;
	*statdata_merged_size = m;

cleanup:
	if (copy_a)
		free(copy_a);
	if (copy_b)
		free(copy_b);

	if (retval != 0)
	{
		if (*statdata_merged)
			free(*statdata_merged);
		*statdata_merged = NULL;
		*statdata_merged_size = 0;
	}
	else
	{
		STATDATA_LOG("successfully joined two array of statdata");
	}

	return retval;
}

static inline void
MergeStatdataEntries(StatData *dst, const StatData *src)
{
	assert(dst->id == src->id);
	dst->count += src->count;
	dst->cost += src->cost;
	dst->primary &= src->primary;
	dst->mode =
		dst->mode > src->mode ? dst->mode : src->mode;
}

static bool
IsSortedById(const StatData *statdata, size_t statdata_size)
{
	for (size_t i = 1; i < statdata_size; i++)
	{
		if (statdata[i-1].id > statdata[i].id)
			return false;
	}
	return true;
}

static int
SortDumpByIdComparator(const void *a, const void *b)
{
	const StatData *statdata_a = a;
	const StatData *statdata_b = b;

	if (statdata_a->id > statdata_b->id)
		return 1;
	else if (statdata_a->id < statdata_b->id)
		return -1;
	else
		return 0;
}

void
SortDump(StatData *statdata, size_t statdata_size)
{
	if (statdata_size == 0)
	{
		STATDATA_LOG("array is empty, nothing to sort");
		return;
	}

	assert(statdata != NULL);

	STATDATA_LOG("trying to sort an array of statdata");

	qsort(statdata, statdata_size, sizeof(StatData), SortDumpByCostComparator);

	STATDATA_LOG("successfully sorted the array of statdata");
}

static int
SortDumpByCostComparator(const void *a, const void *b)
{
	const StatData *statdata_a = a;
	const StatData *statdata_b = b;

	if (statdata_a->cost > statdata_b->cost)
		return 1;
	else if (statdata_a->cost < statdata_b->cost)
		return -1;
	else
		return 0;
}

bool
StatdataEqual(const StatData *a, const StatData *b)
{
	return (a->id == b->id &&
			a->count == b->count &&
			a->cost == b->cost &&
			a->primary == b->primary &&
			a->mode == b->mode);
}

void
PrintStatdata(const char *prefix, const StatData *statdata)
{
	STATDATA_LOG("%s: id=%ld count=%d cost=%f primary=%u mode=%u\n",
				 prefix,
				 statdata->id,
				 statdata->count,
				 statdata->cost,
				 statdata->primary,
				 statdata->mode);
}
