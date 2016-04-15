/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <string.h>
#include "conffile.h"

/* Allocate and initialize a struct conffile with the given parameters */
struct conffile *conffile_new(FILE *fp) {
    struct conffile *ret = calloc(1, sizeof(struct conffile));
    if (ret == NULL) return NULL;
    ret->fp = fp;
    return ret;
}

/* Allocate and initialize a struct section with the given parameters */
struct section *section_new(char *name) {
    struct section *ret = calloc(1, sizeof(struct section));
    if (ret == NULL) return NULL;
    ret->name = name;
    return ret;
}

/* Allocate and initialize a struct pair with the given parameters */
struct pair *pair_new(char *key, char *value) {
    struct pair *ret = calloc(1, sizeof(struct pair));
    if (ret == NULL) return NULL;
    ret->key = key;
    ret->value = value;
    return ret;
}

/* Add the given section to the given file */
void conffile_add(struct conffile *file, struct section *section) {
    if (file->sections == NULL) {
        file->sections = section;
        section->prev = section->next = NULL;
    } else {
        section_append(file->sections, section);
    }
}

/* Add the given pair to the given section */
void section_add(struct section *section, struct pair *pair) {
    if (section->data == NULL) {
        section->data = pair;
        pair->prev = pair->next = NULL;
    } else {
        pair_append(section->data, pair);
    }
}

/* Append the new section to a given list, choosing the correct position */
void section_append(struct section *list, struct section *section) {
    struct section *cur, *last = NULL, *found = NULL;
    /* Scan for section with same name */
    for (cur = list; cur != NULL; last = cur, cur = cur->next) {
        /* Take that! */
        if ((section->name == NULL) ? cur->name == NULL :
                strcmp(section->name, cur->name) == 0)
            found = cur;
    }
    /* Yes, this *will* segfault (if used improperly). */
    if (! found) found = last;
    section->next = found->next;
    found->next = section;
    if (section->next) section->next->prev = section;
    section->prev = found;
}

/* Append the new pair to a given list, choosing the correct position */
void pair_append(struct pair *list, struct pair *pair) {
    struct pair *cur, *last = NULL, *found = NULL;
    /* Scan for section with same name */
    for (cur = list; cur != NULL; last = cur, cur = cur->next) {
        /* Take that! */
        if (strcmp(pair->key, cur->key) == 0)
            found = cur;
    }
    /* Yes, all of this *is* copied from above (with minor modifications). */
    if (! found) found = last;
    pair->next = found->next;
    found->next = pair;
    if (pair->next) pair->next->prev = pair;
    pair->prev = found;
}
