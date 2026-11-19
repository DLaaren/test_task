#define _GNU_SOURCE // for PATH_MAX
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "logging.h"
#include "statdata.h"
#include "tests.h"

#define TESTS_DIR_NAME "tests"
#define DEFAULT_PROCESSOR_PATH "./build/processor"

static int JoinPath(char *path, size_t path_size, const char *prefix, const char *suffix);
static double ConvertTimespecToMs(const struct timespec *start);
static int RunProcessor(const char *prog, const char *path_a,
						const char *path_b, const char *path_act);

int
main(int argc, char **argv)
{
	const char *processor_path = (argc > 1) ? argv[1] : DEFAULT_PROCESSOR_PATH;
	const TestData *tests = NULL;
	size_t num_tests = 0;
	StatData *act = NULL;
	size_t act_size = 0;
	double total_ms = 0.0;
	int retval = 0;

	StatDataLogSetFile("tester.log");

	if (mkdir(TESTS_DIR_NAME, 0700) != 0 && errno != EEXIST)
	{
		STATDATA_ERR("could not create directory " TESTS_DIR_NAME": %m");

		retval = -1;
		goto cleanup;
	}

	tests = GetTests(&num_tests);

	for (size_t t = 0; t < num_tests; t++)
	{
		char path_dir[PATH_MAX];
		char path_a[PATH_MAX];
		char path_b[PATH_MAX];
		char path_ref[PATH_MAX];
		char path_act[PATH_MAX];
		struct timespec t_start;
		double test_ms = 0;

		const StatData *a = tests[t].a;
		size_t a_size = tests[t].a_size;
		const StatData *b = tests[t].b;
		size_t b_size = tests[t].b_size;
		const StatData *ref = tests[t].ref;
		size_t ref_size = tests[t].ref_size;

		clock_gettime(CLOCK_MONOTONIC, &t_start);

		/*
		 * XXX: should we validate test data or it's programmer's responsibility
		 * to give the correct ones?
		 */
		snprintf(path_dir, sizeof(path_dir), TESTS_DIR_NAME"/%zu", t+1);
		if (mkdir(path_dir, 0700) != 0 && errno != EEXIST)
		{
			STATDATA_ERR("could not create directory \"%s\": %m", path_dir);

			retval = -1;
			goto cleanup;
		}

		if (JoinPath(path_a, sizeof(path_a), path_dir, "/a") != 0 ||
			JoinPath(path_b, sizeof(path_b), path_dir, "/b") != 0 ||
			JoinPath(path_ref, sizeof(path_ref), path_dir, "/ref") != 0 ||
			JoinPath(path_act, sizeof(path_act), path_dir, "/act") != 0)
		{
			STATDATA_ERR("path too long, exiting");

			retval = -1;
			goto cleanup;
		}

		/*
		 * StoreDump refuses to overwrite an existing file, so drop the
		 * leftovers of a previous tester run -- otherwise the tester
		 * would not be re-runnable without manual cleanup
		 */
		unlink(path_a);
		unlink(path_b);
		unlink(path_ref);
		unlink(path_act);

		if (StoreDump(a, a_size, path_a) != 0 ||
			StoreDump(b, b_size, path_b) != 0 ||
			StoreDump(ref, ref_size, path_ref) != 0)
		{
			STATDATA_ERR("could not store test statdata, exiting");

			retval = -1;
			goto cleanup;
		}

		if (RunProcessor(processor_path, path_a, path_b, path_act) != 0)
		{
			STATDATA_ERR("could not get result from processor, exiting");

			retval = -1;
			goto cleanup;
		}

		if (LoadDump(&act, &act_size, path_act) != 0)
		{
			STATDATA_ERR("could not load test statdata, exiting");

			retval = -1;
			goto cleanup;
		}

		if (act_size != ref_size)
		{
			STATDATA_ERR("test %zu: size mismatch: expected %zu, got %zu",
						 t+1, ref_size, act_size);

			retval = -1;
			goto cleanup;
		}

		for (size_t i = 0; i < act_size; i++)
		{
			if (!StatdataEqual(&act[i], &ref[i]))
			{
				STATDATA_ERR("test %zu: mismatch at index %zu", t + 1, i);
				PrintStatdata("expected", &ref[i]);
				PrintStatdata("actual", &act[i]);

				retval = -1;
				goto cleanup;
			}
		}

		/* report per the task: a list of passed tests on stdout */
		test_ms = ConvertTimespecToMs(&t_start);

		total_ms += test_ms;
		/* print successful result in stdout */
		printf("test %zu: passed (%.2f ms)\n\n", t + 1, test_ms);

		if (act)
			free(act);

		act = NULL;
		act_size = 0;
	}

cleanup:
	if (act)
		free(act);

	if (retval == 0)
		/* print successful result in stdout */
		printf("ok: all %zu tests passed, %.2f ms in total\n",
			   num_tests, total_ms);

	StatDataLogClose();

	return retval;
}

/*
 * run the processor in a child process and wait for it: fork + execv
 * returns 0 iff the processor exited with 0
 * Note: we may also use system() but it would run bash, what creates some
 * overhead
 */
static int
RunProcessor(const char *prog, const char *path_a,
			 const char *path_b, const char *path_act)
{
	pid_t pid = fork();
	int status = 0;

	if (pid < 0)
	{
		STATDATA_ERR("could not fork for the processor: %m");
		return -1;
	}

	if (pid == 0)
	{
		execv(prog,
			  (char *const[]){(char *)prog, (char *)path_a, (char *)path_b,
							  (char *)path_act, NULL});
		/* shouldn't be here. */
		STATDATA_ERR("execv \"%s\" failed: %m", prog);
		StatDataLogClose();

		_exit(127);
	}

	/* parent */
	if (waitpid(pid, &status, 0) < 0)
	{
		STATDATA_ERR("could not wait for the processor: %m");
		return -1;
	}

	if (!WIFEXITED(status))
	{
		STATDATA_ERR("processor killed by signal %d", WTERMSIG(status));
		return -1;
	}

	if (WEXITSTATUS(status) != 0)
	{
		STATDATA_ERR("processor exited with status %d", WEXITSTATUS(status));
		return -1;
	}

	return 0;
}

static double
ConvertTimespecToMs(const struct timespec *start)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - start->tv_sec) * 1000.0 +
	       (now.tv_nsec - start->tv_nsec) / 1000000.0;
}

static int
JoinPath(char *path, size_t path_size, const char *prefix, const char *suffix)
{
	int n = snprintf(path, path_size, "%s%s", prefix, suffix);

	if (n < 0 || (size_t)n >= path_size)
		return -1;

	return 0;
}
