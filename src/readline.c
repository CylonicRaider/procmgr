/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include "readline.h"

#define DEFAULT_BUFSIZE 128
#define LF '\n'

/* Read a line from f, up to (and including) a LF character */
ssize_t readline(FILE *f, char **buffer, size_t *size) {
    ssize_t buflen = 0;
    int ch, done = 0;
    clearerr(f);
    for (;;) {
        /* (Re)allocate buffer if necessary. */
        if (! *buffer) {
            *size = DEFAULT_BUFSIZE;
            *buffer = malloc(*size);
        } else if (*size <= buflen) {
            *size *= 2;
            *buffer = realloc(*buffer, *size);
        }
        /* Abort if necessary and space for the NUL is there. */
        if (done) break;
        /* Read character. */
        ch = fgetc(f);
        if (ch == EOF) {
            if (ferror(f)) {
                errno = EIO;
                return -1;
            }
            break;
        }
        /* Append to buffer. */
        buffer[buflen++] = ch;
        /* Do not abort here to allow space allocation to run again for the
         * terminating NUL. */
        if (ch == LF) done = 1;
    }
    /* Done. */
    buffer[buflen] = '\0';
    return buflen;
}

/* Remove leading and trailing whitespace from a string */
char *strip_whitespace(char *input, size_t len) {
    /* Find left non-space character. */
    while (len && isspace(input[len - 1])) len--;
    /* Replace character after it with NUL. */
    input[len] = '\0';
    /* Skip leading whitespace. */
    while (*input && isspace(*input)) input++;
    /* Return result. */
    return input;
}
