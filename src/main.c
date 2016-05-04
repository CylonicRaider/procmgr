/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Main program */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

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

/* Global data for signal handlers */
static int sigpipe[2] = { -1, -1 };

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

/* Signal handler */
static void notifier(int signum) {
    unsigned char sn = signum;
    write(sigpipe[1], &sn, 1);
}

/* Server main loop */
int server_main(struct config *config, int background, char *argv[]) {
    struct ctlmsg msg = CTLMSG_INIT;
    struct sigaction act;
    fd_set readfds;
    /* Currently no arguments */
    if (argv && *argv) {
        fprintf(stderr, "Too many arguments\n");
        return 2;
    }
    /* Create communication pipe */
    if (pipe(sigpipe) == -1) {
        perror("Could not create pipe");
        return 1;
    }
    /* Install signal handlers */
    memset(&act, 0, sizeof(act));
    act.sa_handler = notifier;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGHUP, &act, NULL) == -1) {
        perror("Could not install signal handler");
        return 1;
    }
    if (sigaction(SIGTERM, &act, NULL) == -1) {
        perror("Could not install signal handler");
        return 1;
    }
    if (sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("Could not install signal handler");
        return 1;
    }
    /* Create socket */
    if (comm_listen(config) == -1) {
        perror("Could not create socket");
        return 1;
    }
    /* Go into background */
    if (background && daemonize() == -1) {
        perror("Failed to go into background");
        return 1;
    }
    /* Main loop */
    for (;;) {
        int nfds, res;
        struct timeval timeout;
        /* Prepare for select() */
        FD_ZERO(&readfds);
        FD_SET(config->socket, &readfds);
        FD_SET(sigpipe[0], &readfds);
        nfds = (config->socket > sigpipe[0]) ? config->socket : sigpipe[0];
        nfds++;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        /* Determine which event to check now */
        if (select(nfds, &readfds, NULL, NULL, &timeout) == -1) {
            perror("Failed to select()");
            return 1;
        }
        /* Check for signals */
        if (FD_ISSET(sigpipe[0], &readfds)) {
            unsigned char signo;
            if (read(sigpipe[0], &signo, 1) != 1) {
                perror("Failed to read");
                return 1;
            }
            /* Act upon them */
            if (signo == SIGHUP) {
                /* Reload configuration */
                /* Disabled because of dangling pointer hazard
                config_update(config, 0);*/
            } else if (signo == SIGTERM) {
                /* Shut down */
                break;
            } else if (signo == SIGCHLD) {
                int pid, status, retcode;
                /* Wait for children */
                for (;;) {
                    /* Harvest exit codes */
                    pid = waitpid(-1, &status, WNOHANG);
                    if (pid == -1) {
                        if (errno == ECHILD) break;
                        perror("wait() failed");
                        return 1;
                    } else if (pid == 0) {
                        break;
                    }
                    /* Assemble retcode */
                    if (WIFEXITED(status)) {
                        retcode = WEXITSTATUS(status);
                    } else if (WIFSIGNALED(status)) {
                        retcode = -WTERMSIG(status);
                    } else {
                        continue;
                    }
                    /* Run jobs */
                    run_jobs(config, pid, retcode);
                }
            }
        }
        /* Receive message */
        if (FD_ISSET(config->socket, &readfds)) {
            struct ctlmsg msg2 = CTLMSG_INIT;
            struct addr addr;
            int res = comm_recv(config->socket, &msg, &addr);
            if (res == -2) continue;
            if (res == -1) {
                perror("Failed to receive message");
                return 1;
            }
            /* Reject non-repliable messages */
            if (addr.addrlen < sizeof(sa_family_t) ||
                addr.addr.sun_family == AF_UNSPEC) continue;
            /* Act upon them */
            if (msg.fieldnum == 0) {
                /* No command? Cannot really do anything */
                if (comm_senderr(config->socket, "NOMSG", "Empty message",
                                 &addr) == -1) {
                    perror("Failed to send message");
                    goto commerr;
                }
            } else if (strcmp(msg.fields[0], "PING") == 0) {
                /* Reply with a PONG */
                if (msg.fieldnum > 2) {
                    if (comm_senderr(config->socket, "BADMSG", "Bad message",
                                     &addr) == -1) {
                        perror("Failed to send message");
                        goto commerr;
                    }
                } else if (msg.fieldnum == 2) {
                    char *fields[] = { "PONG", msg.fields[1] };
                    msg2.fields = fields;
                    msg2.fieldnum = 2;
                    if (comm_send(config->socket, &msg2, &addr) == -1) {
                        perror("Failed to send message");
                        goto commerr;
                    }
                } else {
                    char *fields[] = { "PONG" };
                    msg2.fields = fields;
                    msg2.fieldnum = 1;
                    if (comm_send(config->socket, &msg2, &addr) == -1) {
                        perror("Failed to send message");
                        goto commerr;
                    }
                }
            } else if (strcmp(msg.fields[0], "SIGNAL") == 0) {
                char *okfields[] = { "OK" };
                /* Signal oneself, or fail */
                if (msg.fieldnum != 2) {
                    if (comm_senderr(config->socket, "BADMSG", "Bad message",
                                     &addr) == -1) {
                        perror("Failed to send message");
                        goto commerr;
                    }
                } else if (strcmp(msg.fields[1], "reload") == 0) {
                    if (raise(SIGHUP) != 0) {
                        perror("Could not signal oneself ?!");
                        goto commerr;
                    }
                } else if (strcmp(msg.fields[1], "shutdown") == 0) {
                    if (raise(SIGINT) != 0) {
                        perror("Could not signal oneself ?!");
                        goto commerr;
                    }
                } else {
                    if (comm_senderr(config->socket, "BADMSG", "Bad message",
                                     &addr) == -1) {
                        perror("Failed to send message");
                        goto commerr;
                    }
                }
                /* Reply with OK
                 * The signal handler will have only written to pipe, so we
                 * can reply safely. */
                msg2.fields = okfields;
                msg2.fieldnum = 1;
                if (comm_send(config->socket, &msg2, &addr) == -1) {
                    perror("Failed to send message");
                    goto commerr;
                }
            } else if (strcmp(msg.fields[0], "RUN") == 0) {
                int res;
                /* Create request */
                struct request *req = request_new(config, &msg, &addr);
                if (req == NULL && errno) {
                    perror("Failed to create request");
                    goto commerr;
                }
                /* Validate it */
                res = request_validate(req);
                if (res == -1) {
                    perror("Failed to validate request");
                    goto commerr;
                }
                if (! res) {
                    if (comm_senderr(config->socket, "EPERM", "Permission denied",
                                     &addr) == -1) {
                        perror("Failed to send message");
                        goto commerr;
                    }
                    continue;
                }
                /* Act as appropriate */
                if (request_run(req) == -1) {
                    perror("Failed to process request");
                    goto commerr;
                }
                /* Leaking request since it is held by the job queue */
            } else {
                if (comm_senderr(config->socket, "BADCMD", "No such command",
                                 &addr) == -1) {
                    perror("Failed to send message");
                    goto commerr;
                }
            }
        }
        /* Run unbound jobs */
        do {
            res = run_jobs(config, -1, JOB_NOEXIT);
            if (res == -1) {
                perror("Callback execution failed");
                return 1;
            }
        } while (res);
    }
    /* Everything went well. :) */
    return 0;
    /* Error happened while processing message */
    commerr:
        comm_del(&msg);
        return 1;
}

/* Client main function */
int client_main(struct config *config, enum cmdaction action, char *argv[]) {
    char *cmd, *param, **data, *buf[3];
    int res, l;
    /* Determine which command to send */
    switch (action) {
        case SPAWN : cmd = "RUN"   ; param = NULL      ; break;
        case RELOAD: cmd = "SIGNAL"; param = "reload"  ; break;
        case TEST  : cmd = "PING"  ; param = NULL      ; break;
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
