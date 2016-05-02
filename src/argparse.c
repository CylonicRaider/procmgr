/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <string.h>

#include "argparse.h"

/* Prepare for argument parsing */
void arginit(struct opt *opt, char *argv[]) {
    memset(opt, 0, sizeof(*opt));
    opt->argv = argv;
    opt->curidx = (*argv) ? 1 : 0;
}

/* Argument parser */
int argparse(struct opt *opt) {
    /* After end of options? Only arguments left, or none. */
    if (opt->argend) {
        return (opt->argv[opt->curidx]) ? 0 : -2;
    }
    /* Search for the next option */
    for (;;) {
        /* Get current argument */
        char *curarg = opt->argv[opt->curidx];
        if (! curarg) return -2;
        /* Special handling of first character */
        if (opt->curchr == 0) {
            if (curarg[0] != '-' || curarg[1] == 0) {
                /* Non-option, or single-hyphen argument */
                return 0;
            } else if (curarg[1] == '-') {
                if (curarg[2] == 0) {
                    /* Argument separator */
                    opt->argend = 1;
                    opt->curidx++;
                    return 0;
                } else {
                    /* Long option */
                    opt->curchr = 2;
                    return (strchr(curarg, '=')) ? -4 : -3;
                }
            } else {
                /* Place curchr to point to option */
                opt->curchr++;
            }
        }
        /* Move to next option if necessary */
        if (! curarg[opt->curchr]) {
            opt->curidx++;
            opt->curchr = 0;
            continue;
        }
        /* Return option character */
        return ((unsigned char) curarg[opt->curchr++]);
    }
}

/* Get argument from struct opt */
char *getarg(struct opt *opt, int optional) {
    /* Current argument */
    char *ret = opt->argv[opt->curidx];
    /* Check for end of command line */
    if (! ret) return NULL;
    /* Skip to current position */
    ret += opt->curchr;
    /* Return next argument if required and current one is empty */
    if (! optional && ! *ret) {
        opt->curidx++;
        opt->curchr = 0;
        ret = opt->argv[opt->curidx];
    }
    /* Advance to next argument */
    opt->curidx++;
    opt->curchr = 0;
    /* Done */
    return ret;
}
