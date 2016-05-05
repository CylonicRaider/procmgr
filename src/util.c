/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#include "util.h"

/* The current UNIX timestamp as a double-precision floating-point value */
double timestamp() {
    struct timeval tv;
    /* Should not fail... :S */
    if (gettimeofday(&tv, NULL) == -1)
        return NAN;
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

/* Fork into background */
int daemonize() {
    int pid = fork();
    if (pid == -1) return -1;
    if (pid != 0) _exit(0);
    if (setsid() == -1) return -1;
    return chdir("/");
}
