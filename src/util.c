/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <math.h>
#include <stddef.h> /* For NULL */
#include <sys/time.h>

#include "util.h"

/* The current UNIX timestamp as a double-precision floating-point value */
double timestamp(void) {
    struct timeval tv;
    /* Should not fail... :S */
    if (gettimeofday(&tv, NULL) == -1)
        return NAN;
    return tv.tv_sec + tv.tv_usec * 1e-6;
}
