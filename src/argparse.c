/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Prepare for option parsing */
void arginit(struct opt *opt, char *argv[]) {
    memset(opt, 0, sizeof(*opt));
    opt->argv = argv;
    opt->curidx = 1;
}

/* Option parser
 * Return value:  1+: That character read as an option.
 *                0 : Non-option argument.
 *               -1 : Error (currently none).
 *               -2 : End of arguments.
 *               -3 : Long option.
 *               -4 : Long option with argument (after '=').
 */
int argparse(struct opt *opt) {
    if (opt->argend) {
        return (opt->argv[opt->curidx]) ? 0 : -2;
    }
    for (;;) {
        char *curarg = opt->argv[opt->curidx];
        if (! curarg) return -2;
        if (opt->curchr == 0) {
            if (curarg[0] != '-' || curarg[1] == 0) {
                return 0;
            } else if (curarg[1] == '-') {
                if (curarg[2] == 0) {
                    opt->argend = 1;
                    opt->curidx++;
                    return 0;
                } else {
                    opt->curchr = 2;
                    return (strchr(curarg, '=')) ? -4 : -3;
                }
            } else {
                opt->curchr++;
            }
        }
        if (! curarg[opt->curchr]) {
            opt->curidx++;
            opt->curchr = 0;
            continue;
        }
        return curarg[opt->curchr++];
    }
}

/* Get argument from struct opt */
char *getarg(struct opt *opt, int optional) {
    char *ret = opt->argv[opt->curidx];
    if (! ret) return ret;
    ret += opt->curchr;
    if (! optional && ! *ret) {
        opt->curidx++;
        opt->curchr = 0;
        ret = opt->argv[opt->curidx];
    }
    opt->curidx++;
    opt->curchr = 0;
    return ret;
}
