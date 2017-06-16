/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
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

/* Parse an integer, optionally interpreting certain keywords */
int parse_int(int *ret, char *data, int keywords) {
    char *end;
    int temp;
    if (keywords & INTKWD_NONE && strcmp(data, "none") == 0) {
        *ret = -1;
        return 1;
    } else if (keywords & INTKWD_YESNO && strcmp(data, "no") == 0) {
        *ret = 0;
        return 1;
    } else if (keywords & INTKWD_YESNO && strcmp(data, "yes") == 0) {
        *ret = 1;
        return 1;
    }
    errno = 0;
    temp = strtol(data, &end, 0);
    if (errno || *end) return 0;
    *ret = temp;
    return 1;
}
