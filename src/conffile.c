/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "readline.h"
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

/* Deinitialize the given structure */
void conffile_del(struct conffile *file) {
    if (file->fp) fclose(file->fp);
    file->fp = NULL;
    if (file->sections) section_free(file->sections);
    file->sections = NULL;
}

/* Deinitialize the given structure */
void section_del(struct section *section) {
    free(section->name);
    if (section->prev) section->prev->next = NULL;
    if (section->next) section->next->prev = NULL;
    section->name = NULL;
    section->prev = NULL;
    section->next = NULL;
    if (section->data) pair_free(section->data);
    section->data = NULL;
}

/* Deinitialize the given structure */
void pair_del(struct pair *pair) {
    free(pair->key);
    free(pair->value);
    if (pair->prev) pair->prev->next = NULL;
    if (pair->next) pair->next->prev = NULL;
    pair->key = NULL;
    pair->value = NULL;
    pair->prev = NULL;
    pair->next = NULL;
}

/* Deinitialize and free the given structure */
void conffile_free(struct conffile *file) {
    conffile_del(file);
    free(file);
}

/* Deinitialize and free the given structure */
void section_free(struct section *section) {
    struct section *prev = section->prev, *next = section->next, *cur;
    while (prev) {
        cur = prev;
        prev = prev->prev;
        section_del(cur);
        free(cur);
    }
    while (next) {
        cur = next;
        next = next->next;
        section_del(cur);
        free(cur);
    }
    section_del(section);
    free(section);
}

/* Deinitialize and free the given structure */
void pair_free(struct pair *pair) {
    struct pair *prev = pair->prev, *next = pair->next, *cur;
    while (prev) {
        cur = prev;
        prev = prev->prev;
        pair_del(cur);
        free(cur);
    }
    while (next) {
        cur = next;
        next = next->next;
        pair_del(cur);
        free(cur);
    }
    pair_del(pair);
    free(pair);
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

/* Get the first section with the given name, or NULL */
struct section *conffile_get(struct conffile *file, char *name) {
    struct section *cur;
    for (cur = file->sections; cur != NULL; cur = cur->next) {
        if ((cur->name == NULL && name == NULL) ||
                (cur->name && name && strcmp(cur->name, name) == 0))
            return cur;
    }
    return NULL;
}

/* Get the last section with the given name, or NULL */
struct section *conffile_get_last(struct conffile *file, char *name) {
    return section_last(conffile_get(file, name));
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

/* Get the first pair with the given key, or NULL */
struct pair *section_get(struct section *section, char *key) {
    struct pair *cur;
    for (cur = section->data; cur != NULL; cur = cur->next) {
        if (strcmp(cur->key, key) == 0)
            return cur;
    }
    return NULL;
}

/* Get the last pair with the given key, or NULL */
struct pair *section_get_last(struct section *section, char *key) {
    return pair_last(section_get(section, key));
}

/* Append the new section to a given list, choosing the correct position */
void section_append(struct section *list, struct section *section) {
    struct section *cur, *last = NULL, *found = NULL;
    /* Scan for section with same name */
    for (cur = list; cur != NULL; last = cur, cur = cur->next) {
        /* Take that! */
        if ((section->name == NULL && cur->name == NULL) ||
                (section->name && cur->name &&
                 strcmp(section->name, cur->name) == 0))
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

/* Return the same-named section preceding this one, if any, or NULL */
struct section *section_prev(struct section *section) {
    struct section *prev = section->prev;
    if (prev == NULL) {
        return NULL;
    } else if (section->name == NULL) {
        return (prev->name == NULL) ? prev : NULL;
    } else {
        return (prev->name != NULL && strcmp(section->name,
            prev->name) == 0) ? prev : NULL;
    }
}

/* Return the same-named section following this one, if any, or NULL */
struct section *section_next(struct section *section) {
    struct section *next = section->next;
    if (next == NULL) {
        return NULL;
    } else if (section->name == NULL) {
        return (next->name == NULL) ? next : NULL;
    } else {
        return (next->name != NULL && strcmp(section->name,
            next->name) == 0) ? next : NULL;
    }
}

/* Return the first same-named section in the current sequence */
struct section *section_first(struct section *section) {
    struct section *prev = section;
    if (section == NULL) return NULL;
    do {
        section = prev;
        prev = section_prev(section);
    } while (prev != NULL);
    return section;
}

/* Return the last same-named section in the current sequence */
struct section *section_last(struct section *section) {
    struct section *next = section;
    if (section == NULL) return NULL;
    do {
        section = next;
        next = section_next(section);
    } while (next != NULL);
    return section;
}

/* Return the same-keyed pair preceding this one, if any, or NULL */
struct pair *pair_prev(struct pair *pair) {
    struct pair *prev = pair->prev;
    return (prev != NULL && strcmp(pair->key, prev->key) == 0) ? prev : NULL;
}

/* Return the same-named pair following this one, if any, or NULL */
struct pair *pair_next(struct pair *pair) {
    struct pair *next = pair->next;
    return (next != NULL && strcmp(pair->key, next->key) == 0) ? next : NULL;
}

/* Return the first same-named pair in the current sequence */
struct pair *pair_first(struct pair *pair) {
    struct pair *prev = pair;
    if (pair == NULL) return NULL;
    do {
        pair = prev;
        prev = pair_prev(pair);
    } while (prev != NULL);
    return pair;
}

/* Return the last same-named pair in the current sequence */
struct pair *pair_last(struct pair *pair) {
    struct pair *next = pair;
    if (pair == NULL) return NULL;
    do {
        pair = next;
        next = pair_next(pair);
    } while (next != NULL);
    return pair;
}

/* Parse the file of the given struct conffile */
int conffile_parse(struct conffile *file, int *curline) {
    char *buffer = NULL, *line, *eq;
    size_t buflen, linelen;
    int ret = 0;
    struct conffile curfile = { NULL, NULL };
    struct section cursec = { NULL, NULL, NULL, NULL }, *section;
    struct pair curpair = { NULL, NULL, NULL, NULL }, *pair;
    /* Initialize curline */
    if (curline) *curline = 0;
    /* Read the file linewise. */
    for (;;) {
        if (curline) (*curline)++;
        /* Actually read line. */
        linelen = readline(file->fp, &buffer, &buflen);
        if (linelen == 0) break;
        if (linelen == -1) goto error;
        /* Check for embedded NUL-s. */
        if (strlen(buffer) != linelen) {
            errno = EINVAL;
            goto error;
        }
        /* Remove whitespace. */
        line = strip_whitespace(buffer);
        linelen = strlen(line);
        /* Lines from here on are "significant". */
        ret++;
        /* Ignore empty lines and comments. */
        if (linelen == 0 || line[0] == '#' || line[0] == ';')
            continue;
        /* Check for section idenfitier. */
        if (line[0] == '[' && line[linelen - 1] == ']') {
            /* Drain section into file structure. */
            section = malloc(sizeof(cursec));
            if (section == NULL) goto error;
            *section = cursec;
            conffile_add(&curfile, section);
            /* Start new section. */
            cursec.data = NULL;
            line[linelen - 1] = '\0';
            cursec.name = strdup(line + 1);
            if (! cursec.name) goto error;
            continue;
        }
        /* Parse a key-value pair. */
        eq = strchr(line, '=');
        if (eq == NULL) {
            errno = EINVAL;
            goto error;
        }
        *eq++ = '\0';
        /* Extract key and value. */
        curpair.key = strdup(strip_whitespace(line));
        if (curpair.key == NULL) goto error;
        curpair.value = strdup(strip_whitespace(eq));
        if (curpair.value == NULL) goto error;
        /* Add to current section. */
        pair = malloc(sizeof(curpair));
        if (pair == NULL) goto error;
        *pair = curpair;
        section_add(&cursec, pair);
        curpair.key = NULL;
        curpair.value = NULL;
    }
    /* Add last section to file as well. */
    section = malloc(sizeof(cursec));
    if (section == NULL) goto error;
    *section = cursec;
    conffile_add(file, section);
    cursec.data = NULL;
    cursec.name = NULL;
    /* Swap old configuration with new one */
    if (file->sections)
        section_free(file->sections);
    file->sections = curfile.sections;
    goto end;
    /* An error happened. */
    error:
        ret = -1;
    end:
        /* Deallocate structures if necessary. */
        free(buffer);
        conffile_del(&curfile);
        section_del(&cursec);
        pair_del(&curpair);
        return ret;
}

/* Write the given set of configuration data to the given I/O stream */
int conffile_write(FILE *file, struct conffile *cfile) {
    int written = 0, w;
    struct section *cur;
    for (cur = cfile->sections; cur != NULL; cur = cur->next) {
        w = section_write(file, cur);
        if (w < 0) return -1;
        written += w;
    }
    return written;
}

/* Write the given section (with a header if necessary) to file */
int section_write(FILE *file, struct section *section) {
    int written = 0, w;
    struct pair *cur;
    if (section->name) {
        w = fprintf(file, "\n[%s]\n", section->name);
        if (w < 0) return -1;
        written += w;
    }
    for (cur = section->data; cur != NULL; cur = cur->next) {
        w = pair_write(file, cur);
        if (w < 0) return -1;
        written += w;
    }
    return written;
}

/* Write the given configuration pair to file */
int pair_write(FILE *file, struct pair *pair) {
    return fprintf(file, "%s=%s\n", pair->key, pair->value);
}
