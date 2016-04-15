/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Configuration file parser
 * Configuration files consist of lines, which can be blank, comments,
 * section introducers, or value assignments.
 * Leading and trailing whitespace (including the line terminator) is
 * ignored.
 * Blank lines contain nothing and are ignored.
 * Comments are introduced by number signs (#) or semicolons (;), and are
 * ignored as well.
 * Section introducers consist of an arbitrary identifier enclosed in square
 * brackets; value assignments after a section introducer are counted towards
 * that section; value assignments before an introducer count towards a
 * special "global" section. Multiple same-named sections are possible; they
 * are stored in order, and can be accessed individually.
 * Value assignments consist of an arbitrary key (which must not include
 * equals signs (=)), which is followed by an arbitrary value (which may
 * contain equals signs). Leading and trailing whitespace is ignored in both
 * names and values. Multiple assignments with the same key are as possible
 * as multiple sections with the same name.
 * Yes, this *is* yet another derivate of the INI file format.
 */

#ifndef _CONFFILE_H
#define _CONFFILE_H

#include <stdio.h>

struct section;
struct pair;

/* Configuration file
 * Members:
 * fp      : (FILE *) A stdio file for (re-)reading the configuration. May be
 *           NULL if no file present.
 * sections: (struct section *) The first section contained in the
 *           configuration file. May be NULL if none present.
 */
struct conffile {
    FILE *fp;
    struct section *sections;
};

/* Section of a configuration file
 * Members:
 * name      : (char *) The NUL-terminated name of this section. May be NULL
 *             for the "global" section.
 * prev, next: (struct section *) Linked list interconnection.
 * data      : (struct pair *) The first data item in this section.
 */
struct section {
    char *name;
    struct section *prev, *next;
    struct pair *data;
};

/* A single name-value pair
 * Members:
 * key       : (char *) The key of this pair.
 * value     : (char *) The value of this pair.
 * prev, next: (struct pair *) Linked list interconnection.
 */
struct pair {
    char *key;
    char *value;
    struct pair *prev, *next;
};

#endif
