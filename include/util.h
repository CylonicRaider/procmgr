/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Miscellaneous utilities */

#ifndef _UTIL_H
#define _UTIL_H

#define INTKWD_NONE  1 /* "none" is mapped to -1 */
#define INTKWD_YESNO 2 /* "yes" maps to 1, "no" maps to 0 */

/* The current UNIX timestamp as a double-precision floating-point value */
double timestamp(void);

/* Fork into background */
int daemonize(void);

/* Parse an integer, optionally interpreting certain keywords
 * ret is a pointer to the integer that will contain the value if the call
 * succeeds; value is the string to parse. keywords is a bitmask of INTKWD_*
 * constants.
 * Returns nonzero in case of success and zero otherwise, with errno set.
 */
int parse_int(int *ret, char *value, int keywords);

#endif
