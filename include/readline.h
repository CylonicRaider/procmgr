/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Line extraction from stdio files */

#ifndef _READLINE_H
#define _READLINE_H

#include <stddef.h>
#include <stdio.h>

/* Read a line from f, up to (and including) a LF character
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

/* Remove leading and trailing whitespace from a string
 * Trailing whitespace is chopped off by replacing the first character of it
 * with a NUL byte (which may be the character immediately after the string
 * if there is no trailing whitespace -- this can be used to extract NUL-
 * terminated portions from larger buffers); leading whitespace is handled by
 * returning a pointer to the first non-whitespace character.
 * len specifies the length of the string, and may be used to select
 * substrings. Embedded NUL-s are ignored; thus, determining the length of
 * the substring would require about as much effort as writing a custom
 * variant of this function.
 * Returns a pointer to the beginning of the trimmed string.
 */
char *strip_whitespace(char *input, size_t len);

#endif
