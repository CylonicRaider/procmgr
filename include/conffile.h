/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Configuration file parser
 * Configuration files consist of lines, which can be blank, comments,
 * section introducers, or value assignments.
 * Leading and trailing whitespace (including the line terminator) is
 * ignored.
 * Blank lines contain nothing and are ignored.
 * Comments are introduced by number signs (#) or semicolons (;), and are
 * ignored as well. Comments must be on their own lines.
 * Section introducers consist of an arbitrary identifier enclosed in square
 * brackets; value assignments after a section introducer are counted towards
 * that section; value assignments before an introducer count towards a
 * special "global" section. Multiple same-named sections are possible; they
 * are stored in order, and can be accessed individually.
 * Value assignments consist of an arbitrary key (which must not include
 * equals signs (=)), which is followed by an arbitrary value (which may
 * contain equals signs). Keys and values can be empty. Leading and trailing
 * whitespace is ignored in both names and values. Multiple assignments with
 * the same key are as possible as multiple sections with the same name.
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
 * Returns a pointer the newly-created structure, or NULL if allocation
 * fails. */
struct conffile *conffile_new(FILE *fp);

/* Allocate and initialize a struct section with the given parameters
 * Returns a pointer the newly-created structure, or NULL if allocation
 * fails. */
struct section *section_new(char *name);

/* Allocate and initialize a struct pair with the given parameters
 * Returns a pointer the newly-created structure, or NULL if allocation
 * fails. */
struct pair *pair_new(char *key, char *value);

/* Deinitialize the given structure
 * The backing file (if any) is closed; extract it manually to prevent
 * that. */
void conffile_del(struct conffile *file);

/* Deinitialize the given structure
 * Linked list references to section are reset to NULL. */
void section_del(struct section *section);

/* Deinitialize the given structure
 * Linked list references to pair are reset to NULL. */
void pair_del(struct pair *pair);

/* Deinitialize and free the given structure */
void conffile_free(struct conffile *file);

/* Deinitialize and free the given structure
 * Any linked sections are disposed of as well. */
void section_free(struct section *section);

/* Deinitialize and free the given structure
 * Any linked pairs are disposed of as well. */
void pair_free(struct pair *pair);

/* Add the given section to the given file */
void conffile_add(struct conffile *file, struct section *section);

/* Add the given pair to the given section */
void section_add(struct section *section, struct pair *pair);

/* Append the new section to a given list, choosing the correct position */
void section_append(struct section *list, struct section *section);

/* Append the new pair to a given list, choosing the correct position */
void pair_append(struct pair *list, struct pair *pair);

/* Parse the file of the given struct conffile
 * The old data in file are deallocated first. If the same should be parsed
 * multiple times, it has to be rewound before repeated parsings.
 * If curline is not NULL, it will be set the 1-based number of the line
 * currently being parsed; in case of errors appearing, it can be used to
 * provide more meaningful information.
 * The return value is the amount of "meaningful" (i.e., non-empty
 * non-comemnt) lines read, or -1 on error with errno set (possible errors
 * include EIO if something happens while reading itself, ENOMEM in case of
 * an OOM, or EINVAL if the syntax is invalid).
 * NOTE that syntax errors are considered fatal; in case those (or any other
 *      errors) appear, only the sections completely successfully parsed
 *      are included in file. */
int conffile_parse(struct conffile *file, int *curline);

#endif
