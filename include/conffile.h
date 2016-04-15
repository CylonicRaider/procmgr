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
#include <stdlib.h>

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
 * name      : (char *) The name of this section. May be NULL for the
 *             "global" section.
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
 * key       : (char *) The key of this pair. Must not be NULL.
 * value     : (char *) The value of this pair. Must not be NULL.
 * prev, next: (struct pair *) Linked list interconnection.
 */
struct pair {
    char *key, *value;
    struct pair *prev, *next;
};

/* Allocate and initialize a struct conffile with the given parameters
 * Returns a pointer the newly-created structure, or NULL if allocation fails
 */
struct conffile *conffile_new(FILE *fp);

/* Allocate and initialize a struct section with the given parameters
 * Returns a pointer the newly-created structure, or NULL if allocation fails
 */
struct section *section_new(char *name);

/* Allocate and initialize a struct pair with the given parameters
 * Returns a pointer the newly-created structure, or NULL if allocation fails
 */
struct pair *pair_new(char *key, char *value);

/* Add the given section to the given file */
void conffile_add(struct conffile *file, struct section *section);

/* Add the given pair to the given section */
void section_add(struct section *section, struct pair *pair);

/* Append the new section to a given list, choosing the correct position */
void section_append(struct section *list, struct section *section);

/* Append the new pair to a given list, choosing the correct position */
void pair_append(struct pair *list, struct pair *pair);

#endif
