#ifndef LOGGING_H
#define LOGGING_H

#include <stdbool.h>

/*
 * Logging goes to the log file set via StatDataLogSetFile(), or to stderr
 * if no file is set; every line is prefixed with a wall-clock timestamp
 * and flushed to the file at the moment it is printed.
 *
 * STATDATA_LOG = informational message;
 * STATDATA_ERR = error;
 */
#define STATDATA_LOG(FMT, ...) \
	StatDataLog("LOG", FMT, ##__VA_ARGS__)
#define STATDATA_ERR(FMT, ...) \
	StatDataLog("ERROR", FMT, ##__VA_ARGS__)

/*
 * start writing log lines to the file `path` (created if missing and
 * truncated: each process gets a fresh log, so re-runs do not accumulate);
 * if the file cannot be opened, logging continues to stderr only
 */
extern void StatDataLogSetFile(const char *path);
/*
 * flush and close the log file (if any); logging continues to stderr only
 */
extern void StatDataLogClose(void);
/*
 * write one log line
 */
extern void StatDataLog(const char *prefix, const char *fmt, ...);

#endif /* LOGGING_H */
