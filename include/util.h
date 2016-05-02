/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Miscellaneous utilities */

#ifndef _UTIL_H
#define _UTIL_H

/* The current UNIX timestamp as a double-precision floating-point value */
double timestamp(void);

/* Fork into background */
int daemonize(void);

#endif
