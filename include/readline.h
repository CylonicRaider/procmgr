/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#ifndef _READLINE_H
#define _READLINE_H

#include <stddef.h>
#include <stdio.h>

/* Read a line from f, up to (and including) a LF character.
 * buffer points to a dynamically allocated buffer; the line is stored at its
 * beginning; size is its size; the buffer is reallocated as necessary (in
 * particular, a buffer of NULL causes a new one to be allocated, with size
 * ignored). size is updated to match the buffer size. After usage, buffer
 * must be free()-d by the caller.
 * If reading reaches EOF, there is no LF at the end of the line.
 * The return value is the length of the line (which may include NUL-s),
 * including the terminator, if any (and excluding the NUL byte added after
 * it). On error, -1 is returned, and errno is set appropriately.
 */
ssize_t readline(FILE *f, char **buffer, size_t *size);

#endif
