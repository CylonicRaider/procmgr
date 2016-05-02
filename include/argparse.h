/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Command-line parsing (because getopt sucks)
 * After initializing a struct opt with arginit(), use argparse() to read
 * option characters, and getarg() to read arguments (for options or
 * non-options) or long option names (and arguments). See argparse() for
 * details.
 * The struct opt may be freely disposed of (as long as it isn't used
 * after that). */

#ifndef _ARGPARSE_H
#define _ARGPARSE_H

/* Status of argument parser
 * Members:
 * argv  : (char **) Argument array.
 * curidx: (int) Current argument being parsed.
 * curchr: (int) Current character inside argument.
 * argend: (int) End of argument list reached. */
struct opt {
    char **argv;
    int curidx;
    int curchr;
    int argend;
};

/* Prepare for argument parsing
 * Since argv is NULL-terminated, an argc is not required. */
void arginit(struct opt *opt, char *argv[]);

/* Parse arguments
 * The structure is advanced as necessary.
 * Return value:
 *  1+: That character read as an option.
 *      Call getarg() to acquire as many arguments as necessary.
 *  0 : Non-option argument.
 *      Call getarg() to obtain it.
 * -1 : Error (currently none).
 * -2 : End of arguments.
 *      Option parsing may (and actually has to) stop here.
 * -3 : Long option.
 *      getarg() returns the option name; another getarg() may read an
 *      argument if necessary.
 * -4 : Long option with argument (after '=').
 *      getarg() returns the option name and the option value (with the
 *      equals sign); those must be extracted manually if necessary. */
int argparse(struct opt *opt);

/* Get another argument from a struct opt
 * If optional is false, skips to the next argument if the current one
 * is exhausted by option parsing. If optional is true, the empty string
 * is returned at the end of an option.
 * Returns the argument string, or NULL if the end of options is reached.
 * Always advances the structure such that a next argparse() or getarg()
 * call will read the argument following the returned one. */
char *getarg(struct opt *opt, int optional);

#endif
