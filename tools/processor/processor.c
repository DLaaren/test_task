#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "logging.h"
#include "statdata.h"

/* how many records are printed into the table */
#define TABEL_TOP_NUM 10

static void PrintTopTable(const StatData *statdata, size_t statdata_size);
static inline void Usage(const char *progname);
static void FormatCost(char *buf, size_t buf_size, float cost);
static void FormatMode(char *buf, unsigned int mode);
static void PhaseStop(struct timespec *t_start, double *ms);

/*
 * measure the phase that started at *t_start: set *ms to its duration
 * and move the start point to now, so the next phase can be measured
 */
static void
PhaseStop(struct timespec *t_start, double *ms)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	*ms = (now.tv_sec - t_start->tv_sec) * 1000.0 +
	       (now.tv_nsec - t_start->tv_nsec) / 1000000.0;
	*t_start = now;
}

int
main(int argc, char **argv)
{
	const char *path_a = NULL,
			   *path_b = NULL,
			   *path_merged = NULL;
	StatData   *statdata_a = NULL,
			   *statdata_b = NULL,
			   *statdata_merged = NULL;
	size_t	    statdata_a_size = 0,
			    statdata_b_size = 0,
			    statdata_merged_size = 0;
	struct timespec t_phase;
	double	ms_load_a = 0.0, ms_load_b = 0.0, ms_join = 0.0,
			ms_sort = 0.0, ms_store = 0.0;
	int retval = 0;

	/*
	 * first two args are paths to statdata files;
	 * the third one is path to statdata result file
	 */
	if (argc != 4)
	{
		Usage(argv[0]);
		return -1;
	}

	path_a = argv[1];
	path_b = argv[2];
	path_merged = argv[3];

	StatDataLogSetFile("processor.log");

	clock_gettime(CLOCK_MONOTONIC, &t_phase);
	if (LoadDump(&statdata_a, &statdata_a_size, path_a) != 0)
	{
		STATDATA_ERR("could not load statdata from \"%s\", exiting", path_a);

		retval = -1;
		goto cleanup;
	}
	PhaseStop(&t_phase, &ms_load_a);

	if (LoadDump(&statdata_b, &statdata_b_size, path_b) != 0)
	{
		STATDATA_ERR("could not load statdata from \"%s\", exiting", path_b);

		retval = -1;
		goto cleanup;
	}
	PhaseStop(&t_phase, &ms_load_b);

	if (JoinDump(statdata_a, statdata_a_size, statdata_b, statdata_b_size,
				 &statdata_merged, &statdata_merged_size) != 0)
	{
		STATDATA_ERR("could not join statdata, exiting");

		retval = -1;
		goto cleanup;
	}
	PhaseStop(&t_phase, &ms_join);

	SortDump(statdata_merged, statdata_merged_size);
	PhaseStop(&t_phase, &ms_sort);

	PrintTopTable(statdata_merged, statdata_merged_size);

	if (StoreDump(statdata_merged, statdata_merged_size, path_merged) != 0)
	{
		STATDATA_LOG("could not store statdata to \"%s\", exiting", path_merged);

		retval = -1;
		goto cleanup;
	}
	PhaseStop(&t_phase, &ms_store);

	STATDATA_LOG("timing: load a %.2f ms, load b %.2f ms, "
				 "join %.2f ms, sort %.2f ms, store %.2f ms, "
				 "total %.2f ms",
				 ms_load_a, ms_load_b, ms_join, ms_sort, ms_store,
				 ms_load_a + ms_load_b + ms_join + ms_sort + ms_store);

cleanup:

	if (statdata_a)
		free(statdata_a);
	if (statdata_b)
		free(statdata_b);
	if (statdata_merged)
		free(statdata_merged);

	StatDataLogClose();

	return retval;
}

/*
 * prints the first TABEL_TOP_NUM records of the cost-sorted array as a
 * table:
 *   id      -- hex
 *   count   -- decimal
 *   cost    -- scientific, 3 digits after the dot
 *   primary -- "n" if 0, "y" if 1
 *   mode    -- 3 binary digits
 */
static void
PrintTopTable(const StatData *statdata, size_t statdata_size)
{
	const size_t rows_num =
		statdata_size < TABEL_TOP_NUM ? statdata_size : TABEL_TOP_NUM;
	char cost_buf[16];
	char mode_buf[4];

	/* header of the table: fixed-width, right-aligned columns */
	printf("%20s %11s %16s %8s %8s\n",
		   "id", "count", "cost", "primary", "mode");

	for (size_t i = 0; i < rows_num; i++)
	{
		FormatCost(cost_buf, sizeof(cost_buf), statdata[i].cost);
		FormatMode(mode_buf, statdata[i].mode);

		printf("%20lx %11d %16s %8s %8s\n",
			   (unsigned long)statdata[i].id,
			   statdata[i].count,
			   cost_buf,
			   statdata[i].primary ? "y" : "n",
			   mode_buf);
	}
}

static inline void
Usage(const char *progname)
{
	fprintf(stderr, "usage: %s <path_a> <path_b> <path_merged>\n", progname);
}

/*
 * formats cost in scientific notation with 3 digits after the decimal
 * point and an exponent without leading zeros, per the task requirements:
 * 123 -> "1.230e+2", 3e10 -> "3.000e+10"
 *
 * %.3e always prints the exponent with exactly two digits (for float the
 * exponent is always within [-45, 38]), so the leading zero is dropped
 * when present: "1.230e+02" -> "1.230e+2"; an exponent without a leading
 * zero must be kept intact: "3.000e+10" stays "3.000e+10"
 */
static void
FormatCost(char *buf, size_t buf_size, float cost)
{
	char *exponent;

	snprintf(buf, buf_size, "%.3e", cost);

	exponent = strchr(buf, 'e');
	if (exponent == NULL)
		return; /* inf/nan -- nothing to reformat */

	if (exponent[2] == '0')
	{
		exponent[2] = exponent[3];
		exponent[3] = '\0';
	}
}

/*
 * formats the 3-bit mode field as a binary string: 5 -> "101"
 */
static void
FormatMode(char *buf, unsigned int mode)
{
	/* little endian */
	for (ssize_t bit = 2; bit >= 0; bit--)
		buf[2 - bit] = '0' + ((mode >> bit) & 1);
	buf[3] = '\0';
}
