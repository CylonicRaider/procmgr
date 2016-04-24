/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"

/* Static functions/constants */
static int parse_int(int *ret, char *value);
static struct action **action_pointer(struct program *prog, char *name);
static void action_free(struct action **act);

static struct actionname {
    char *base;
    char *cmd;
    char *uid;
    char *gid;
} action_names[] = {
    { "start",   "cmd-start",   "uid-start",   "gid-start"   },
    { "restart", "cmd-restart", "uid-restart", "gid-restart" },
    { "reload",  "cmd-reload",  "uid-reload",  "gid-reload"  },
    { "signal",  "cmd-signal",  "uid-signal",  "gid-signal"  },
    { "stop",    "cmd-stop",    "uid-stop",    "gid-stop"    },
    { "status",  "cmd-status",  "uid-status",  "gid-status"  } };
#define action_count (sizeof(action_names) / sizeof(*action_names))

/* Create a new runtime configuration based on the given configuration file */
struct config *config_new(struct conffile *file, int quiet) {
    struct config *ret = calloc(1, sizeof(struct config));
    if (ret == NULL) {
        if (! quiet) perror("Error while allocating structure");
        return NULL;
    }
    ret->socketpath = strdup(SOCKET_PATH);
    if (ret->socketpath == NULL) {
        if (! quiet) perror("Failed to allocate string");
        config_free(ret);
        return NULL;
    }
    ret->socket = -1;
    ret->conffile = file;
    if (config_update(ret, quiet) == -1) {
        ret->conffile = NULL;
        config_free(ret);
        return NULL;
    }
    return ret;
}

/* Deallocate all the underlying structures */
void config_del(struct config *conf) {
    if (conf->socket != -1) {
        close(conf->socket);
        if ((conf->flags & CONFIG_UNLINK) && conf->socketpath) {
            int en = errno;
            unlink(conf->socketpath);
            errno = en;
        }
    }
    conf->socket = -1;
    conf->flags = 0;
    if (conf->conffile) conffile_free(conf->conffile);
    conf->conffile = NULL;
    if (conf->programs) prog_free(conf->programs);
    conf->programs = NULL;
}

/* Deallocate all the underlying structures and free this one */
void config_free(struct config *conf) {
    config_del(conf);
    free(conf);
}

/* Re-read the underlying configuration file and merge the new configuration
 * with the current one */
int config_update(struct config *conf, int quiet) {
    struct pair *pair;
    struct section *sec;
    struct program *prog, *nextprog;
    int ret = 0;
    /* No file present -> Nothing to do */
    if (! conf->conffile) return 0;
    /* Re-parse configuration file */
    if (conf->conffile->fp) {
        int lineno = -1;
        /* Rewind */
        clearerr(conf->conffile->fp);
        rewind(conf->conffile->fp);
        if (ferror(conf->conffile->fp)) {
            if (! quiet) perror("Could not rewind configuration file (?!)");
            return -1;
        }
        /* Parse */
        if (conffile_parse(conf->conffile, &lineno) == -1) {
            if (! quiet)
                fprintf(stderr, "Could not parse configuration file "
                    "(line %d): %s\n", lineno, strerror(errno));
            return -1;
        }
    }
    /* Reset global members */
    if (! conf->socketpath || strcmp(conf->socketpath, SOCKET_PATH) == 0) {
        free(conf->socketpath);
        conf->socketpath = strdup(SOCKET_PATH);
        if (! conf->socketpath) {
            if (! quiet) perror("Could not allocate string");
            return -1;
        }
    }
    conf->def_uid = -1;
    conf->def_gid = -1;
    /* Parse global members */
    sec = conffile_get_last(conf->conffile, NULL);
    if (sec) {
        int value;
        /* Configuration socket path */
        pair = section_get_last(sec, "socket-path");
        if (pair) {
            if (conf->socketpath) free(conf->socketpath);
            conf->socketpath = strdup(pair->value);
            if (! conf->socketpath) {
                if (! quiet) perror("Could not allocte string");
                return -1;
            }
        }
        /* Default UID */
        pair = section_get_last(sec, "allow-uid");
        if (pair) {
            if (! parse_int(&value, pair->value)) {
                if (! quiet) perror("Could not parse default UID");
                return -1;
            }
            conf->def_uid = value;
        }
        /* Default GID */
        pair = section_get_last(sec, "allow-gid");
        if (pair) {
            if (! parse_int(&value, pair->value)) {
                if (! quiet) perror("Could not parse default GID");
                return -1;
            }
            conf->def_gid = value;
        }
    }
    /* Mark all programs for removal (merged ones will have flag clear) */
    for (prog = conf->programs; prog != NULL; prog = prog->next) {
        prog->flags |= PROG_REMOVE;
    }
    /* Reload programs */
    for (sec = conf->conffile->sections; sec != NULL; sec = sec->next) {
        /* Scroll to last section of "grop" */
        sec = section_last(sec);
        /* Ignore not appropriately named sections */
        if (sec->name == NULL || strncmp(sec->name, "prog-", 5) != 0)
            continue;
        /* Create program */
        prog = prog_new(conf, sec);
        if (! prog) {
            if (! quiet)
                fprintf(stderr, "Could not create program structure (%s): "
                    "%s\n", sec->name + 5, strerror(errno));
            return -1;
        }
        /* Merge with old one, if any */
        config_add(conf, prog);
        ret++;
    }
    /* Remove programs not present anymore */
    for (prog = conf->programs; prog != NULL; prog = nextprog) {
        nextprog = prog->next;
        if (! (prog->flags & PROG_REMOVE) || prog->pid) continue;
        if (prog == conf->programs) {
            conf->programs = prog->next;
            conf->programs->prev = NULL;
        } else {
            prog->prev->next = prog->next;
            if (prog->next) prog->next->prev = prog->prev;
        }
        prog_del(prog);
        free(prog);
    }
    /* Done */
    return ret;
}

/* Add the given program to the configuration, merging the entries if
 * necessary */
void config_add(struct config *conf, struct program *prog) {
    struct program *old, *prev = NULL;
    /* Find old location */
    for (old = conf->programs; old != NULL; prev = old, old = old->next) {
        if (strcmp(old->name, prog->name) == 0) break;
    }
    /* Add new entry */
    if (! old) {
        if (prev == NULL) {
            conf->programs = prog;
            prog->prev = NULL;
            prog->next = NULL;
        } else {
            prev->next = prog;
            prog->prev = prev;
            prog->next = NULL;
        }
        return;
    }
    /* Re-link the structure */
    if (old == conf->programs) {
        conf->programs = prog;
        prog->prev = NULL;
        prog->next = old->next;
    } else {
        old->prev->next = prog;
        if (old->next) old->next->prev = prog;
        prog->prev = old->prev;
        prog->next = old->next;
    }
    old->prev = NULL;
    old->next = NULL;
    /* Migrate PID and flags */
    prog->flags = old->flags & ~PROG_REMOVE;
    prog->pid = old->pid;
    /* Deallocate old structure */
    prog_del(old);
    free(old);
}

/* Return the program named by the given string, or NULL if none */
struct program *config_get(struct config *conf, char *name) {
    struct program *cur;
    for (cur = conf->programs; cur != NULL; cur = cur->next) {
        if (strcmp(cur->name, name) == 0) break;
    }
    return cur;
}

/* Remove the given program from the configuration, deallocating it */
void config_remove(struct config *conf, struct program *prog) {
    if (conf->programs == prog) conf->programs = prog->next;
    prog_del(prog);
    free(prog);
}

/* Allocate a program using the configuration from the given configuration
 * section (which may be NULL) */
struct program *prog_new(struct config *conf, struct section *config) {
    struct pair *pair;
    struct program *ret = calloc(1, sizeof(struct program));
    struct action *act = NULL;
    int i, def_uid, def_gid;
    /* Set name */
    if (config->name == NULL) {
        ret->name = strdup("");
    } else if (strncmp(config->name, "prog-", 5) == 0) {
        ret->name = strdup(config->name + 5);
    } else {
        ret->name = strdup(config->name);
    }
    if (! ret->name) goto error;
    /* Obtain default UID and GID */
    def_uid = (conf == NULL) ? -1 : conf->def_uid;
    def_gid = (conf == NULL) ? -1 : conf->def_gid;
    /* Read configuration */
    if (config) {
        /* Update default UID and GID */
        pair = section_get_last(config, "allow-uid");
        if (pair && ! parse_int(&def_uid, pair->value)) goto error;
        pair = section_get_last(config, "allow-gid");
        if (pair && ! parse_int(&def_gid, pair->value)) goto error;
        /* Initialize actions */
        for (i = 0; i < action_count; i++) {
            /* Set command */
            pair = section_get_last(config, action_names[i].cmd);
            if (! pair) continue;
            act = calloc(1, sizeof(struct action));
            if (! act) goto error;
            act->command = strdup(pair->value);
            if (act->command == NULL) goto error;
            /* Set up UID and GID */
            act->allow_uid = def_uid;
            act->allow_gid = def_gid;
            pair = section_get_last(config, action_names[i].uid);
            if (pair && ! parse_int(&act->allow_uid, pair->value))
                goto error;
            pair = section_get_last(config, action_names[i].gid);
            if (pair && ! parse_int(&act->allow_gid, pair->value))
                goto error;
            /* Insert into structure */
            *action_pointer(ret, action_names[i].base) = act;
        }
    }
    /* Done */
    goto end;
    error:
        free(act);
        prog_free(ret);
        ret = NULL;
    end:
        return ret;
}

/* Free all the resources underlying the given structure */
void prog_del(struct program *prog) {
    if (prog->name) free(prog->name);
    prog->pid = -1;
    prog->flags = 0;
    prog->delay = 0;
    if (prog->prev) prog->prev->next = prog->next;
    if (prog->next) prog->next->prev = prog->prev;
    prog->prev = NULL;
    prog->next = NULL;
    action_free(&prog->act_start);
    action_free(&prog->act_restart);
    action_free(&prog->act_reload);
    action_free(&prog->act_signal);
    action_free(&prog->act_stop);
    action_free(&prog->act_status);
}

/* Deallocate the given structure, as well as any others linked to it */
void prog_free(struct program *prog) {
    struct program *prev = prog->prev, *next = prog->next, *cur;
    while (prev) {
        cur = prev;
        prev = prev->prev;
        prog_del(cur);
        free(cur);
    }
    while (next) {
        cur = next;
        next = next->next;
        prog_del(cur);
        free(cur);
    }
    prog_del(prog);
    free(prog);
}

/* Parse an integer literal ("none" maps to -1)
 * Returns whether successful; errno is set if not. */
int parse_int(int *ret, char *data) {
    char *end;
    int temp;
    if (strcmp(data, "none") == 0) {
        *ret = -1;
        return 1;
    }
    errno = 0;
    temp = strtol(data, &end, 0);
    if (errno || *end) return 0;
    *ret = temp;
    return 1;
}

/* Return the action named by name from prog, or NULL if none */
struct action *prog_action(struct program *prog, char *name) {
    struct action **ptr = action_pointer(prog, name);
    return (ptr) ? *ptr : NULL;
}

/* Get a pointer to the struct action corresponding to the name, or NULL if
 * none */
struct action **action_pointer(struct program *prog, char *name) {
    if (strcmp(name, "start") == 0) {
        return &prog->act_start;
    } else if (strcmp(name, "restart") == 0) {
        return &prog->act_restart;
    } else if (strcmp(name, "reload") == 0) {
        return &prog->act_reload;
    } else if (strcmp(name, "signal") == 0) {
        return &prog->act_signal;
    } else if (strcmp(name, "stop") == 0) {
        return &prog->act_stop;
    } else if (strcmp(name, "status") == 0) {
        return &prog->act_status;
    } else {
        return NULL;
    }
}

/* Deallocate the given action object and clear the reference */
static void action_free(struct action **act) {
    if (! *act) return;
    if ((*act)->command) free((*act)->command);
    free(*act);
    *act = NULL;
}
