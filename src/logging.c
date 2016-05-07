/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "logging.h"

#define TMBUFLEN 48

/* Static information */
static struct {
    FILE *fp;
    struct logging_syslog *syslog;
} loginfo = { NULL, NULL };

static struct {
    char *name;
    int priority;
} levels[] = {
    { "DEBUG", LOG_DEBUG },
    { "INFO", LOG_INFO },
    { "NOTE", LOG_NOTICE },
    { "WARN", LOG_WARNING },
    { "ERROR", LOG_ERR },
    { "CRITICAL", LOG_CRIT },
    { "FATAL", LOG_ALERT }
};

static char *weekdays[] = { "Sun", "Mon", "Tue", "Wed",
                            "Thu", "Fri", "Sat" };

/* Format a timestamp */
static void format_ts(char *buf) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, TMBUFLEN, "??? %Y-%m-%d %H:%M:%S %z", tm);
    memcpy(buf, weekdays[tm->tm_wday], 3);
}

/* Initialize logging */
int initlog(FILE *fp, struct logging_syslog *syslog) {
    loginfo.fp = fp;
    loginfo.syslog = syslog;
    if (syslog)
        openlog(syslog->ident, syslog->option, syslog->facility);
    /* Currently no errors */
    return 0;
}

/* Log the string */
void logmsg(int level, char *message) {
    FILE *fp = loginfo.fp;
    if (level >= FATAL && ! fp) fp = stderr;
    if (fp) {
        char ts[TMBUFLEN];
        format_ts(ts);
        fprintf(fp, "[%s] %s\n", ts, message);
    }
    if (loginfo.syslog) {
        syslog(levels[level].priority, "%s", message);
    }
}

/* Log the string, along with strerror(errno) appended after a colon
 * (similarly to perror()) */
void logerr(int level, char *message) {
    FILE *fp = loginfo.fp;
    if (level >= FATAL && ! fp) fp = stderr;
    if (fp) {
        char ts[TMBUFLEN];
        int en = errno;
        format_ts(ts);
        fprintf(fp, "[%s] %s: %s\n", ts, message, strerror(en));
        errno = en;
    }
    if (loginfo.syslog) {
        syslog(levels[level].priority, "%s: %m", message);
    }
}

/* De-initialize the logging machinery */
void quitlog(void) {
    loginfo.fp = NULL;
    loginfo.syslog = NULL;
}
