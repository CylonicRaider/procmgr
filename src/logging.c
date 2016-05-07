/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "logging.h"

#define TMBUFLEN 48

/* Static information */
static struct logleveldesc {
    char *name;
    int level;
    int priority;
} levels[] = {
    { "DEBUG"   , DEBUG   , LOG_DEBUG   },
    { "INFO"    , INFO    , LOG_INFO    },
    { "NOTE"    , NOTE    , LOG_NOTICE  },
    { "WARN"    , WARN    , LOG_WARNING },
    { "ERROR"   , ERROR   , LOG_ERR     },
    { "CRITICAL", CRITICAL, LOG_CRIT    },
    { "FATAL"   , FATAL   , LOG_ALERT   },
    { NULL      , -1      , -1          }
};

static struct facility {
    char *name;
    int facility;
} facilities[] = {
    { "KERN"    , LOG_KERN     },
    { "USER"    , LOG_USER     },
    { "MAIL"    , LOG_MAIL     },
    { "NEWS"    , LOG_NEWS     },
    { "UUCP"    , LOG_UUCP     },
    { "DAEMON"  , LOG_DAEMON   },
    { "AUTH"    , LOG_AUTH     },
    { "AUTHPRIV", LOG_AUTHPRIV },
    { "CRON"    , LOG_CRON     },
    { "LPR"     , LOG_LPR      },
    { "FTP"     , LOG_FTP      },
    { "SYSLOG"  , LOG_SYSLOG   },
    { "LOCAL0"  , LOG_LOCAL0   },
    { "LOCAL1"  , LOG_LOCAL1   },
    { "LOCAL2"  , LOG_LOCAL2   },
    { "LOCAL3"  , LOG_LOCAL3   },
    { "LOCAL4"  , LOG_LOCAL4   },
    { "LOCAL5"  , LOG_LOCAL5   },
    { "LOCAL6"  , LOG_LOCAL6   },
    { "LOCAL7"  , LOG_LOCAL7   },
    { NULL      , -1           }
};

static char *weekdays[] = { "Sun", "Mon", "Tue", "Wed",
                            "Thu", "Fri", "Sat" };

/* Global state */
static struct {
    FILE *fp;
    struct logging_syslog *syslog;
    int level;
} loginfo = { NULL, NULL, INT_MAX };


/* Format a timestamp */
static void format_ts(char *buf) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, TMBUFLEN, "??? %Y-%m-%d %H:%M:%S %z", tm);
    memcpy(buf, weekdays[tm->tm_wday], 3);
}

/* Initialize logging */
int initlog(FILE *fp, struct logging_syslog *syslog, int level) {
    loginfo.fp = fp;
    loginfo.syslog = syslog;
    if (syslog)
        openlog(syslog->ident, syslog->option, syslog->facility);
    loginfo.level = level;
    /* Currently no errors */
    return 0;
}

/* Log the string */
void logmsg(int level, char *message) {
    FILE *fp = loginfo.fp;
    if (level < loginfo.level) return;
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
    if (level < loginfo.level) return;
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

/* Obtain the logging level for the given keyword, or -1 if none */
int loglevel(char *name) {
    struct logleveldesc *p;
    for (p = levels; p->name; p++) {
        if (strcasecmp(name, p->name) == 0) break;
    }
    return p->level;
}

/* Obtain the syslog facility corresponding to name, or -1 if none */
int facility(char *name) {
    struct facility *p;
    for (p = facilities; p->name; p++) {
        if (strcasecmp(name, p->name) == 0) break;
    }
    return p->facility;
}
