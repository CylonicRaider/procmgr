/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Main program */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "argparse.h"
#include "control.h"
#include "main.h"
#include "util.h"

/* Usage and help */
const char *USAGE = "USAGE: " PROGNAME " [-h|-V] [-c conffile] [-d "
    "[-f]|-t|-s|-r]\n";
const char *HELP =
    "-h: (--help) This help\n"
    "-V: (--version) Print version (" VERSION ")\n"
    "-c: (--config) Configuration file location (defaults to environment\n"
    "    variable PROCMGR_CONFFILE, or to " DEFAULT_CONFFILE " if not\n"
    "    set)\n"
    "-d: (--daemon) Start daemon (as opposed to the default \"client\"\n"
    "    mode)\n"
    "-f: (--foreground) Stay in foreground (daemon mode only)\n"
    "-t: (--test) Check whether the daemon is running\n"
    "-s: (--stop) Signal the daemon (if any running) to stop\n"
    "-r: (--reload) Signal the daemon (if any running) to reload its\n"
    "    configuration\n";

/* Allocate a configuration given a filename */
struct config *create_config(char *filename) {
    struct config *config;
    struct conffile *conffile;
    FILE *fp = fopen(filename, "r");
    if (! fp) return NULL;
    conffile = conffile_new(fp);
    if (! conffile) goto ferror;
    config = config_new(conffile, 0);
    if (! config) goto cnferror;
    return config;
    ferror:
        fclose(fp);
        return NULL;
    cnferror:
        conffile_free(conffile);
        return NULL;
}

/* Abort program execution with the given error message along with
 * strerror(errno) */
void die(char *func) {
    perror(func);
    exit(1);
}

/* Write a usage message */
void usage(int help, int retcode) {
    fputs(USAGE, stderr);
    if (help) fputs(HELP, stderr);
    exit(retcode);
}

/* Server main loop */
int server_main(struct config *config, int background, char *argv[]) {
    /* TODO */
    fprintf(stderr, "NYI\n");
    return 3;
}

/* Client main function */
int client_main(struct config *config, enum cmdaction action, char *argv[]) {
    char *cmd, *param, **data, *buf[3];
    int res, l;
    /* Determine which command to send */
    switch (action) {
        case SPAWN : cmd = "RUN"   ; param = NULL;       break;
        case RELOAD: cmd = "SIGNAL"; param = "reload";   break;
        case TEST  : cmd = "PING"  ; param = NULL;       break;
        case STOP  : cmd = "SIGNAL"; param = "shutdown"; break;
        default:
            fprintf(stderr, "Internal error\n");
            return 1;
    }
    /* Prepend command to arguments */
    if (action == SPAWN) {
        l = 2;
        for (data = argv; data && *data; data++) l++;
        data = calloc(l, sizeof(char *));
        if (! data) {
            perror("Failed to allocate memory");
            return 1;
        }
        data[0] = cmd;
        if (argv) memcpy(data + 1, argv, l * sizeof(char *));
    } else {
        data = buf;
        data[0] = cmd;
        data[1] = param;
        data[2] = NULL;
    }
    /* Connect */
    if (comm_connect(config) == -1) {
        perror("Could not connect");
        return 1;
    }
    /* Send command */
    res = send_request(config, data);
    if (action == SPAWN) free(data);
    if (res == 0) {
        fprintf(stderr, "Invalid arguments\n");
        return 2;
    } else if (res == -1) {
        perror("Error while sending command");
        return 1;
    }
    /* Obtain reply */
    res = get_reply(config);
    if (res == REPLY_ERROR) {
        perror("Error while receiving reply");
        return 1;
    }
    return res;
}

/* Main function */
int main(int argc, char *argv[]) {
    int server = 0, background = -1, ret;
    char *conffile = NULL, **args = NULL;
    enum cmdaction action = SPAWN;
    struct opt opts;
    struct config *config;
    /* Parse arguments */
    arginit(&opts, argv);
    for (;;) {
        int opt = argparse(&opts);
        if (opt == 0) {
            args = opts.argv + opts.curidx;
            break;
        } else if (opt == -1) {
            die("Internal error");
        } else if (opt == -2) {
            break;
        } else if (opt == -3) {
            char *arg = getarg(&opts, 0);
            if (strcmp(arg, "help") == 0) {
                usage(1, 0);
            } else if (strcmp(arg, "version") == 0) {
                puts(PROGNAME " " VERSION);
                return 0;
            } else if (strcmp(arg, "config") == 0) {
                conffile = getarg(&opts, 0);
                if (! conffile) {
                    fprintf(stderr, "Missing required argument for '%s'\n",
                            arg);
                    usage(0, 2);
                }
            } else if (strcmp(arg, "daemon") == 0) {
                server = 1;
            } else if (strcmp(arg, "foreground") == 0) {
                background = 0;
            } else if (strcmp(arg, "test") == 0) {
                action = TEST;
            } else if (strcmp(arg, "stop") == 0) {
                action = STOP;
            } else if (strcmp(arg, "reload") == 0) {
                action = RELOAD;
            } else {
                fprintf(stderr, "Unknown option: '--%s'\n", arg);
                return 1;
            }
            continue;
        } else if (opt == -4) {
            char *arg = getarg(&opts, 0);
            char *eq = strchr(arg, '=');
            *eq++ = '\0';
            if (strcmp(arg, "config") == 0) {
                conffile = eq;
            } else {
                fprintf(stderr, "Unknown option: '--%s'\n", arg);
                usage(0, 2);
            }
            continue;
        }
        switch (opt) {
            case 'h':
                usage(1, 0);
            case 'V':
                puts(PROGNAME " " VERSION);
                return 0;
            case 'c':
                conffile = getarg(&opts, 0);
                if (! conffile) {
                    fprintf(stderr, "Missing required argument for '-%c'\n",
                            opt);
                    usage(0, 2);
                }
                break;
            case 'd':
                server = 1;
                break;
            case 'f':
                background = 0;
                break;
            case 't':
                action = TEST;
                break;
            case 's':
                action = STOP;
                break;
            case 'r':
                action = RELOAD;
                break;
            default:
                fprintf(stderr, "Unknown option: '-%c'\n", opt);
                usage(0, 2);
        }
    }
    /* Finish options; validate them */
    if (background == -1) background = server;
    if (server && action != SPAWN) {
        fprintf(stderr, "Both daemon mode and an action specified\n");
        return 2;
    }
    /* Fill in configuration file */
    if (! conffile) {
        conffile = getenv("PROCMGR_CONFFILE");
        if (conffile) {
            conffile = strdup(conffile);
            if (! conffile) die("Could not allocate string");
        }
    }
    if (! conffile) conffile = DEFAULT_CONFFILE;
    /* Create configuration */
    config = create_config(conffile);
    if (! config) die("Failed to load configuration");
    /* Main... branch */
    if (server) {
        ret = server_main(config, background, args);
    } else {
        ret = client_main(config, action, args);
    }
    /* Clean up */
    config_free(config);
    /* Done */
    return ret;
}
