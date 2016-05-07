/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Logging
 * Logging happens to a file descriptor, to a syslog facility, or to
 * any combination of those.
 * If no file descriptor is configured, fatal messages are logged to
 * stderr (independently from the syslog configuration). */

#ifndef _LOGGING_H
#define _LOGGING_H

/* Syslog configuration
 * For details on the members, see the manual of openlog(3).
 * Members:
 * ident   : (char *) A program identifier.
 * option  : (int) Misnomer for some "flags".
 * facility: (int) The facility to log to. */
struct logging_syslog {
    char *ident;
    int option;
    int facility;
};

/* Logging levels
 * The levels correspond to the syslog(3) levels (with LOG_EMERG omitted
 * since procmgr is assumed not to be used for mission-critical use). */
enum loglevel { DEBUG, INFO, NOTE, WARN, ERROR, CRITICAL, FATAL };

/* Initialize logging
 * fp is a file to write logging messages to, syslog is either NULL or
 * contains configuration parameters for syslog initialization.
 * The data passed is stored statically; it is never deallocated (so,
 * the caller should take care of disposing of the structures when they
 * are not needed).
 * Returns zero in case of success, or -1 on failure (with errno set
 * appropriately, although no error conditions are known as of now). */
int initlog(FILE *fp, struct logging_syslog *syslog);

/* Log the string
 * level is one of the LEVEL_* constants. */
void logmsg(int level, char *message);

/* Log the string, along with strerror(errno) appended after a colon
 * (similarly to perror())
 * level is one of the LEVEL_* constants. */
void logerr(int level, char *message);

/* De-initialize the logging machinery */
void quitlog(void);

#endif
