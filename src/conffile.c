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
    if (! ret) return NULL;
    ret->fp = fp;
    return ret;
}

/* Allocate and initialize a struct section with the given parameters */
struct section *section_new(char *name) {
    struct section *ret = calloc(1, sizeof(struct section));
    if (! ret) return NULL;
    ret->name = name;
    return ret;
}

/* Allocate and initialize a struct pair with the given parameters */
struct pair *pair_new(char *key, char *value) {
    struct pair *ret = calloc(1, sizeof(struct pair));
    if (! ret) return NULL;
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
    section->name = NULL;
    if (section->prev) section->prev->next = section->next;
    if (section->next) section->next->prev = section->prev;
    section->prev = NULL;
    section->next = NULL;
    if (section->data) pair_free(section->data);
    section->data = NULL;
}

/* Deinitialize the given structure */
void pair_del(struct pair *pair) {
    free(pair->key);
    free(pair->value);
    pair->key = NULL;
    pair->value = NULL;
    if (pair->prev) pair->prev->next = pair->next;
    if (pair->next) pair->next->prev = pair->prev;
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
    struct section *prev, *next, *cur;
    for (cur = section->prev; cur; cur = prev) {
        prev = cur->prev;
        section_del(cur);
        free(cur);
    }
    for (cur = section->next; cur; cur = next) {
        next = cur->next;
        section_del(cur);
        free(cur);
    }
    section_del(section);
    free(section);
}

/* Deinitialize and free the given structure */
void pair_free(struct pair *pair) {
    struct pair *prev, *next, *cur;
    for (cur = pair->prev; cur; cur = prev) {
        prev = cur->prev;
        pair_del(cur);
        free(cur);
    }
    for (cur = pair->next; cur; cur = next) {
        next = cur->next;
        pair_del(cur);
        free(cur);
    }
    pair_del(pair);
    free(pair);
}

/* Add the given section to the given file */
void conffile_add(struct conffile *file, struct section *section) {
    if (! file->sections) {
        file->sections = section;
        section->prev = section->next = NULL;
    } else {
        section_append(file->sections, section);
    }
}

/* Get the first section with the given name, or NULL */
struct section *conffile_get(struct conffile *file, char *name) {
    struct section *cur;
    for (cur = file->sections; cur; cur = cur->next) {
        if ((! cur->name && ! name) ||
                (cur->name && name && strcmp(cur->name, name) == 0))
            return cur;
    }
    return NULL;
}

/* Get the last section with the given name, or NULL */
struct section *conffile_get_last(struct conffile *file, char *name) {
    return section_last(conffile_get(file, name));
}

/* Remove the given section from the given configuration and deallocate it */
void conffile_remove(struct conffile *file, struct section *sec) {
    if (file->sections == sec) file->sections = sec->next;
    section_del(sec);
    free(sec);
}

/* Add the given pair to the given section */
void section_add(struct section *section, struct pair *pair) {
    if (! section->data) {
        section->data = pair;
        pair->prev = pair->next = NULL;
    } else {
        pair_append(section->data, pair);
    }
}

/* Get the first pair with the given key, or NULL */
struct pair *section_get(struct section *section, char *key) {
    struct pair *cur;
    for (cur = section->data; cur; cur = cur->next) {
        if (strcmp(cur->key, key) == 0)
            return cur;
    }
    return NULL;
}

/* Get the last pair with the given key, or NULL */
struct pair *section_get_last(struct section *section, char *key) {
    return pair_last(section_get(section, key));
}

/* Remove the given pair from the given section and free it */
void section_remove(struct section *section, struct pair *pair) {
    if (section->data == pair) section->data = pair->next;
    pair_del(pair);
    free(pair);
}

/* Append the new section to a given list, choosing the correct position */
void section_append(struct section *list, struct section *section) {
    struct section *cur, *last = NULL, *found = NULL;
    /* Scan for section with same name */
    for (cur = list; cur; last = cur, cur = cur->next) {
        /* Take that! */
        if ((! section->name && ! cur->name) ||
                (section->name && cur->name &&
                 strcmp(section->name, cur->name) == 0))
            found = cur;
    }
    /* Yes, this *will* segfault (if used improperly) */
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
    for (cur = list; cur; last = cur, cur = cur->next) {
        /* Take that! */
        if (strcmp(pair->key, cur->key) == 0)
            found = cur;
    }
    /* Yes, all of this *is* copied from above (with minor modifications) */
    if (! found) found = last;
    pair->next = found->next;
    found->next = pair;
    if (pair->next) pair->next->prev = pair;
    pair->prev = found;
}

/* Return the same-named section preceding this one, if any, or NULL */
struct section *section_prev(struct section *section) {
    struct section *prev = section->prev;
    if (! prev) {
        return NULL;
    } else if (! section->name) {
        return (! prev->name) ? prev : NULL;
    } else {
        return (prev->name && strcmp(section->name,
            prev->name) == 0) ? prev : NULL;
    }
}

/* Return the same-named section following this one, if any, or NULL */
struct section *section_next(struct section *section) {
    struct section *next = section->next;
    if (! next) {
        return NULL;
    } else if (! section->name) {
        return (! next->name) ? next : NULL;
    } else {
        return (next->name && strcmp(section->name,
            next->name) == 0) ? next : NULL;
    }
}

/* Return the first same-named section in the current sequence */
struct section *section_first(struct section *section) {
    struct section *prev = section;
    if (! section) return NULL;
    do {
        section = prev;
        prev = section_prev(section);
    } while (prev);
    return section;
}

/* Return the last same-named section in the current sequence */
struct section *section_last(struct section *section) {
    struct section *next = section;
    if (! section) return NULL;
    do {
        section = next;
        next = section_next(section);
    } while (next);
    return section;
}

/* Return the same-keyed pair preceding this one, if any, or NULL */
struct pair *pair_prev(struct pair *pair) {
    struct pair *prev = pair->prev;
    return (prev && strcmp(pair->key, prev->key) == 0) ? prev : NULL;
}

/* Return the same-named pair following this one, if any, or NULL */
struct pair *pair_next(struct pair *pair) {
    struct pair *next = pair->next;
    return (next && strcmp(pair->key, next->key) == 0) ? next : NULL;
}

/* Return the first same-named pair in the current sequence */
struct pair *pair_first(struct pair *pair) {
    struct pair *prev = pair;
    if (! pair) return NULL;
    do {
        pair = prev;
        prev = pair_prev(pair);
    } while (prev);
    return pair;
}

/* Return the last same-named pair in the current sequence */
struct pair *pair_last(struct pair *pair) {
    struct pair *next = pair;
    if (! pair) return NULL;
    do {
        pair = next;
        next = pair_next(pair);
    } while (next);
    return pair;
}

/* Parse the file of the given struct conffile */
int conffile_parse(struct conffile *file, int *curline) {
    char *buffer = NULL, *line, *eq;
    size_t buflen, linelen;
    int ret = -1;
    struct conffile curfile = { NULL, NULL };
    struct section cursec = { NULL, NULL, NULL, NULL }, *section;
    struct pair curpair = { NULL, NULL, NULL, NULL }, *pair;
    /* Initialize curline */
    if (curline) *curline = 0;
    /* Read the file linewise */
    for (;;) {
        if (curline) (*curline)++;
        /* Actually read line */
        linelen = readline(file->fp, &buffer, &buflen);
        if (linelen == 0) break;
        if (linelen == -1) goto end;
        /* Check for embedded NUL-s */
        if (strlen(buffer) != linelen) {
            errno = EINVAL;
            goto minerror;
        }
        /* Remove whitespace */
        line = strip_whitespace(buffer);
        linelen = strlen(line);
        /* Lines from here on are "significant" */
        ret++;
        /* Ignore empty lines and comments */
        if (linelen == 0 || line[0] == '#' || line[0] == ';')
            continue;
        /* Check for section idenfitier */
        if (line[0] == '[' && line[linelen - 1] == ']') {
            /* Drain section into file structure */
            section = malloc(sizeof(cursec));
            if (! section) goto end;
            *section = cursec;
            conffile_add(&curfile, section);
            /* Start new section */
            cursec.data = NULL;
            line[linelen - 1] = '\0';
            cursec.name = strdup(line + 1);
            if (! cursec.name) goto end;
            continue;
        }
        /* Parse a key-value pair */
        eq = strchr(line, '=');
        if (! eq) {
            errno = EINVAL;
            goto minerror;
        }
        *eq++ = '\0';
        /* Extract key and value */
        curpair.key = strdup(strip_whitespace(line));
        if (! curpair.key) goto end;
        curpair.value = strdup(strip_whitespace(eq));
        if (! curpair.value) goto end;
        /* Add to current section */
        pair = malloc(sizeof(curpair));
        if (! pair) goto end;
        *pair = curpair;
        section_add(&cursec, pair);
        curpair.key = NULL;
        curpair.value = NULL;
    }
    /* Add last section to file as well */
    section = malloc(sizeof(cursec));
    if (! section) goto end;
    *section = cursec;
    conffile_add(&curfile, section);
    cursec.data = NULL;
    cursec.name = NULL;
    /* Swap old configuration with new one */
    if (file->sections)
        section_free(file->sections);
    file->sections = curfile.sections;
    curfile.sections = NULL;
    ret = 0;
    goto end;
    /* A minor error happened */
    minerror:
        ret = -2;
    end:
        /* Deallocate structures if necessary */
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
    for (cur = cfile->sections; cur; cur = cur->next) {
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
    for (cur = section->data; cur; cur = cur->next) {
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
