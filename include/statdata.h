#ifndef STATDATA_H
#define STATDATA_H

/*
 * Little dev-doc, see also 'notes.md' :
 * 1. StatData is being stored in binary (non-human-readable) format on disk.
 *    A non-empty dump contains firstly the size of stored StatData entries,
 *    then the entries themselves; an empty array is an empty (0-byte) file.
 */

#include <stdbool.h>
#include <stddef.h>

typedef struct StatData
{
	long int id; /* id isn't unique even withing single array */
	int count;
	float cost;
	unsigned int primary:1;
	unsigned int mode:3;
} StatData;

_Static_assert(sizeof(StatData) == 24, "Unexpected StatData struct size");

#define MAX_STATDATA_ARR_SIZE 100000

/*
 * statdata array may be given non-sorted; statdata_size 0 is a valid value
 * and is stored as an empty (0-byte) file;
 * refuses to overwrite an existing file, removes a truncated one on failure
 */
extern int StoreDump(const StatData *statdata, size_t statdata_size, const char *path);
/*
 * statdata array is allocated in the function -- user must free it by himself;
 * may be NULL if the stored array is empty (statdata_size 0 is valid);
 * an empty (0-byte) file is read as an empty array;
 * refuses to read a file whose stored size exceeds MAX_STATDATA_ARR_SIZE
 */
extern int LoadDump(StatData **statdata, size_t *statdata_size, const char *path);
/*
 * entries with the same id must be merged into one;
 * inputs are not mutated -- unsorted ones are sorted in a copy;
 * result is allocated in the function -- user must free it by himself;
 * *statdata_merged is NULL if both input arrays are empty;
 */
extern int JoinDump(const StatData *statdata_a, size_t statdata_a_size,
					 const StatData *statdata_b, size_t statdata_b_size,
					 StatData **statdata_merged, size_t *statdata_merged_size);
/* mutates the given array; sort by the cost value */
extern void SortDump(StatData *statdata, size_t statdata_size);

extern bool StatdataEqual(const StatData *a, const StatData *b);
extern void PrintStatdata(const char *prefix, const StatData *statdata);

#endif /* STATDATA_H */
