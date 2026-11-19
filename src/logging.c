/* localtime_r() is a POSIX extension, hidden by -std=c11 (strict ISO mode) */
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "logging.h"

static FILE *statdata_log_file = NULL;

void
StatDataLogSetFile(const char *path)
{
	StatDataLogClose();

	if (path == NULL)
		return;

	statdata_log_file = fopen(path, "w");
	if (statdata_log_file == NULL)
		STATDATA_ERR("could not open log file \"%s\" (logging to stderr only): %m", path);
}

void
StatDataLog(const char *prefix, const char *fmt, ...)
{
	char line[4096];
	char timebuf[32];
	char msg[4096 - sizeof(timebuf)];
	va_list args;
	struct timespec now;
	struct tm tm;
	FILE *out = NULL;
	int n = 0;

	assert(prefix);
	assert(fmt);

	if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
		now.tv_sec  = time(NULL);
		now.tv_nsec = 0;
	}

	if (localtime_r(&now.tv_sec, &tm) == NULL)
	{
		snprintf(timebuf, sizeof(timebuf), "[time unavailable]");
	}
	else if (strftime(timebuf, sizeof(timebuf), "[%Y-%m-%d %H:%M:%S]", &tm) == 0)
	{
		snprintf(timebuf, sizeof(timebuf), "[time format error]");
	}

	va_start(args, fmt);
	n = vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	if (n < 0)
	{
		/*
		 * substitute a visible placeholder instead of printing garbage or
		 * nothing.
		 */
		snprintf(msg, sizeof(msg), "[message formatting failed]");
		n = (int)strlen(msg);
	}
	else if ((size_t)n >= sizeof(msg))
	{
		/* truncated; emit exactly sizeof(msg)-1 chars. */
		n = (int)sizeof(msg) - 1;
	}

	/* %03ld = ms; %.*s = exactly n chars of the message */
	n = snprintf(line, sizeof(line), "%s.%03ld %s: %.*s",
				 timebuf, now.tv_nsec / 1000000, prefix, n, msg);
	if (n < 0)
	{
		snprintf(line, sizeof(line), "[log line formatting failed]");
	}

	out = (statdata_log_file != NULL) ? statdata_log_file : stderr;

	// to flush line-by-line to avoid loosing several log lines
	if ((fprintf(out, "%s\n", line) < 0 || fflush(out) != 0) &&
		out != stderr)
	{
		fprintf(stderr, "%s\n", line);
		fflush(stderr);
	}
}

void
StatDataLogClose(void)
{
	if (statdata_log_file != NULL)
	{
		if (fclose(statdata_log_file) != 0)
			perror("could not close logfile");
		statdata_log_file = NULL;
	}
}
