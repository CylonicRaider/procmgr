/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Main program */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "argparse.h"
#include "control.h"
#include "logging.h"
#include "main.h"
#include "util.h"

/* Usage and help */
const char *USAGE = "USAGE: " PROGNAME " [-h|-V] [-c conffile] [-l log] [-L "
    "level] [-d [-f]|-t|-s|-r] [program action [args ...]]\n";
const char *HELP =
    "-h: (--help) This help\n"
    "-V: (--version) Print version (" VERSION ")\n"
    "-c: (--config) Configuration file location (defaults to environment\n"
    "    variable PROCMGR_CONFFILE, or to " DEFAULT_CONFFILE " if not\n"
    "    set)\n"
    "-l: (--log log) Syslog facility to log to, or the string \"stderr\".\n"
    "    Facility keywords override each other, \"stderr\" is a flag.\n"
    "-L: (--loglevel level) Minimum severity of messages to log. level is\n"
    "    one of DEBUG, INFO, NOTE (default), WARN, ERROR, CRITICAL, FATAL.\n"
    "-d: (--daemon) Start daemon (as opposed to the default \"client\"\n"
    "    mode)\n"
    "-f: (--foreground) Stay in foreground (daemon mode only)\n"
    "-t: (--test) Check whether the daemon is running\n"
    "-s: (--stop) Signal the daemon (if any running) to stop\n"
    "-r: (--reload) Signal the daemon (if any running) to reload its\n"
    "    configuration\n"
    "If none of -dftsr are supplied, program and action must be present,\n"
    "and contain the program and action to invoke; additional command-line\n"
    "arguments may be passed to those. If no -l option is specified,\n"
    "nothing is logged (except fatal messages, which are always copied to\n"
    "(at least) stderr). Logging happens only in server mode, in client\n"
    "mode, messages are written to stderr.\n";

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

/* Write a log message about the given request */
void log_request(struct request *request) {
    static struct verbinfo {
        char *action;
        char *verb;
    } verbs[] = {
        { "start",   "Starting"   },
        { "restart", "Restarting" },
        { "reload",  "Reloading"  },
        { "signal",  "Signalling" },
        { "stop",    "Stopping"   },
        { "status",  NULL         },
        { NULL,      NULL         }
    };
    char msgbuf[512];
    struct verbinfo *p;
    /* Find correct verb */
    for (p = verbs; p->action; p++) {
        if (strcmp(request->action->name, p->action) == 0) break;
    }
    /* Cancel if action not found or not logged */
    if (! p->verb) return;
    /* Format message */
    snprintf(msgbuf, sizeof(msgbuf), "%s program '%.128s' on behalf "
        "of {PID=%d,UID=%d,GID=%d}", p->verb, request->program->name,
        request->creds.pid, request->creds.uid, request->creds.gid);
    /* Output it */
    logmsg(INFO, msgbuf);
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
    int res = write(sigpipe[1], &sn, 1);
    /* YES, I'm USING it. */
    (void) res;
}

/* Server main loop */
int server_main(struct config *config, int background, char *pidfile,
                char *argv[]) {
    struct ctlmsg msg = CTLMSG_INIT;
    struct sigaction act;
    int ret = 1;
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
    if (sigaction(SIGINT, &act, NULL) == -1) {
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
    /* Write PID file */
    if (pidfile) {
        char pidbuf[32];
        int pidf;
        memset(pidbuf, 0, sizeof(pidbuf));
        snprintf(pidbuf, sizeof(pidbuf), "%d\n", getpid());
        pidbuf[sizeof(pidbuf) - 1] = '\0';
        pidf = open(pidfile, O_WRONLY | O_CREAT, 0644);
        if (pidf == -1) {
            logerr(ERROR, "Could not open PID file");
        } else {
            int l = strlen(pidbuf);
            if (write(pidf, pidbuf, l) != l) {
                logerr(ERROR, "Could not write PID file");
            }
            close(pidf);
        }
    }
    /* Final preparations */
    FD_ZERO(&readfds);
    logmsg(NOTE, PROGNAME " started");
    /* Main loop */
    for (;;) {
        int nfds, res;
        struct timeval timeout;
        /* Prepare for select() */
        FD_SET(config->socket, &readfds);
        FD_SET(sigpipe[0], &readfds);
        nfds = (config->socket > sigpipe[0]) ? config->socket : sigpipe[0];
        nfds++;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        /* Determine which event to check now */
        res = select(nfds, &readfds, NULL, NULL, &timeout);
        if (res == -1) {
            if (errno == EINTR) {
                FD_ZERO(&readfds);
            } else {
                logerr(FATAL, "Failed to select()");
                return 1;
            }
        }
        /* Check for signals */
        if (FD_ISSET(sigpipe[0], &readfds)) {
            unsigned char signo;
            if (read(sigpipe[0], &signo, 1) != 1) {
                logerr(FATAL, "Failed to read");
                return 1;
            }
            /* Act upon them */
            if (signo == SIGHUP) {
                /* Reload configuration */
                logmsg(NOTE, "Reloading configuration...");
                config_update(config, 0);
                logmsg(INFO, "Done");
            } else if (signo == SIGINT || signo == SIGTERM) {
                /* Shut down */
                logmsg(WARN, "Exiting!");
                break;
            } else if (signo == SIGCHLD) {
                int pid, status, retcode;
                /* Wait for children */
                for (;;) {
                    struct program *prog;
                    /* Harvest exit codes */
                    pid = waitpid(-1, &status, WNOHANG);
                    if (pid == -1) {
                        if (errno == ECHILD) break;
                        logerr(FATAL, "wait() failed");
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
                    /* Obtain program */
                    prog = config_getpid(config, pid);
                    if (prog) {
                        char msgbuf[320];
                        snprintf(msgbuf, sizeof(msgbuf),
                            "Program '%.192s' (%d) exit with status %d%s",
                            prog->name, prog->pid, retcode,
                            (prog->delay > 0 && prog->flags & PROG_RUNNING) ?
                                "; will restart" : "");
                        prog->pid = -1;
                        logmsg(NOTE, msgbuf);
                    }
                    /* Run jobs */
                    run_jobs(config, pid, retcode);
                    /* Restart automatically, if applicable */
                    if (prog && prog->delay > 0) {
                        struct request *req = request_synth(config, prog,
                            "start", NULL);
                        if (! req) {
                            logerr(FATAL, "Failed to allocate request");
                            goto commerr;
                        }
                        req->flags |= REQUEST_DIHNTR;
                        if (! request_schedule(req, timestamp() +
                                               prog->delay)) {
                            request_free(req);
                            logerr(FATAL, "Failed to schedule request");
                            goto commerr;
                        }
                    }
                }
            }
        }
        /* Receive message */
        if (FD_ISSET(config->socket, &readfds)) {
            char *fields[] = { NULL, NULL, NULL };
            struct ctlmsg msg2 = CTLMSG_INIT;
            struct addr addr;
            int res = comm_recv(config->socket, &msg, &addr, COMM_DONTWAIT);
            if (res == -2) continue;
            if (res == -1) {
                logerr(FATAL, "Failed to receive message");
                return 1;
            }
            /* Reject non-repliable messages */
            if (addr.addrlen < sizeof(sa_family_t) ||
                addr.addr.sun_family == AF_UNSPEC) continue;
            /* Act upon them */
            if (msg.fieldnum == 0) {
                /* No command? Cannot really do anything */
                if (comm_senderr(config->socket, "NOMSG", "Empty message",
                                 &addr, COMM_DONTWAIT) == -1) {
                    logerr(FATAL, "Failed to send message");
                    goto commerr;
                }
            } else if (strcmp(msg.fields[0], "PING") == 0) {
                /* Reply with a PONG */
                if (msg.fieldnum > 2) {
                    if (comm_senderr(config->socket, "BADMSG", "Bad message",
                                     &addr, COMM_DONTWAIT) == -1) {
                        logerr(FATAL, "Failed to send message");
                        goto commerr;
                    }
                } else if (msg.fieldnum == 2) {
                    fields[0] = "PONG";
                    fields[1] = msg.fields[1];
                } else {
                    fields[0] = "PONG";
                }
            } else if (strcmp(msg.fields[0], "SIGNAL") == 0) {
                /* Signal oneself, or fail */
                if (msg.fieldnum != 2) {
                    if (comm_senderr(config->socket, "BADMSG", "Bad message",
                                     &addr, COMM_DONTWAIT) == -1) {
                        logerr(FATAL, "Failed to send message");
                        goto commerr;
                    }
                } else if (msg.creds.uid != 0 &&
                           msg.creds.uid != geteuid()) {
                    if (comm_senderr(config->socket, "EPERM",
                                     "Permission denied", &addr,
                                     COMM_DONTWAIT) == -1) {
                        logerr(FATAL, "Failed to send message");
                        goto commerr;
                    }
                } else if (strcmp(msg.fields[1], "reload") == 0) {
                    char msgbuf[128];
                    snprintf(msgbuf, sizeof(msgbuf), "Reloading on behalf "
                        "of {PID=%d,UID=%d,GID=%d}", msg.creds.pid,
                        msg.creds.uid, msg.creds.gid);
                    logmsg(NOTE, msgbuf);
                    if (raise(SIGHUP) != 0) {
                        logerr(FATAL, "Could not signal oneself ?!");
                        goto commerr;
                    }
                    fields[0] = "OK";
                } else if (strcmp(msg.fields[1], "shutdown") == 0) {
                    char msgbuf[128];
                    snprintf(msgbuf, sizeof(msgbuf), "Stopping on behalf "
                        "of {PID=%d,UID=%d,GID=%d}", msg.creds.pid,
                        msg.creds.uid, msg.creds.gid);
                    logmsg(NOTE, msgbuf);
                    if (raise(SIGTERM) != 0) {
                        logerr(FATAL, "Could not signal oneself ?!");
                        goto commerr;
                    }
                    fields[0] = "OK";
                } else {
                    if (comm_senderr(config->socket, "BADMSG", "Bad message",
                                     &addr, COMM_DONTWAIT) == -1) {
                        logerr(FATAL, "Failed to send message");
                        goto commerr;
                    }
                }
                /* The signal handler will have only written to pipe, so we
                 * can reply safely. */
            } else if (strcmp(msg.fields[0], "RUN") == 0) {
                int res;
                /* Create request */
                struct request *req = request_new(config, &msg, &addr,
                                                  COMM_DONTWAIT);
                if (req == NULL) {
                    if (! errno) continue;
                    logerr(FATAL, "Failed to create request");
                    goto commerr;
                }
                /* Validate it */
                res = request_validate(req);
                if (res == -1) {
                    logerr(FATAL, "Failed to validate request");
                    goto commerr;
                }
                if (! res) {
                    if (comm_senderr(config->socket, "EPERM", "Permission denied",
                                     &addr, COMM_DONTWAIT) == -1) {
                        logerr(FATAL, "Failed to send message");
                        goto commerr;
                    }
                    continue;
                }
                /* Drop a note */
                log_request(req);
                /* Act as appropriate */
                if (request_run(req) == -1) {
                    logerr(FATAL, "Failed to process request");
                    goto commerr;
                }
                /* Dispose of request */
                request_free(req);
            } else {
                if (comm_senderr(config->socket, "BADCMD", "No such command",
                                 &addr, COMM_DONTWAIT) == -1) {
                    logerr(FATAL, "Failed to send message");
                    goto commerr;
                }
            }
            /* Common replying code */
            if (fields[0]) {
                msg2.fieldnum = 0;
                while (fields[msg2.fieldnum]) msg2.fieldnum++;
                msg2.fields = fields;
                if (comm_send(config->socket, &msg2, &addr,
                              COMM_DONTWAIT) == -1) {
                    logerr(FATAL, "Failed to send message");
                    goto commerr;
                }
            }
            /* Clean message */
            comm_del(&msg);
        }
        /* Run unbound jobs */
        do {
            res = run_jobs(config, -1, JOB_NOEXIT);
            if (res == -1) {
                logerr(FATAL, "Callback execution failed");
                return 1;
            }
        } while (res);
    }
    /* Everything went well. :) */
    ret = 0;
    goto end;
    /* Error happened while processing message */
    commerr:
        comm_del(&msg);
    end:
        /* Remove PID file, if any */
        if (pidfile && unlink(pidfile) == -1)
            logerr(ERROR, "Could not remove PID file");
        return ret;
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
        if (argv) for (data = argv; *data; data++) l++;
        data = calloc(l, sizeof(char *));
        if (! data) {
            perror("Failed to allocate memory");
            return 1;
        }
        data[0] = cmd;
        if (argv) memcpy(data + 1, argv, (l - 2) * sizeof(char *));
    } else {
        data = buf;
        data[0] = cmd;
        data[1] = param;
        data[2] = NULL;
        if (argv && *argv) {
            fprintf(stderr, "Excess arguments on command line\n");
            return 2;
        }
    }
    /* Connect */
    if (comm_connect(config) == -1) {
        perror("Could not connect");
        return 1;
    }
    /* Send command */
    res = send_request(config, data, 0);
    if (action == SPAWN) free(data);
    if (res == 0) {
        fprintf(stderr, "Invalid arguments\n");
        return 2;
    } else if (res == -1) {
        perror("Error while sending command");
        return 1;
    }
    /* Obtain reply */
    res = get_reply(config, 0);
    if (res == REPLY_ERROR) {
        if (errno) {
            perror("Error while receiving reply");
            return 1;
        } else {
            res = 1;
        }
    } else if (action == TEST) {
        if (res == 0) {
            printf("running\n");
            fflush(stdout);
        } else {
            printf("experiencing problems\n");
            fflush(stdout);
        }
    }
    return res;
}

/* Main function */
int main(int argc, char *argv[]) {
    int server = 0, background = -1, ret;
    char *conffile = NULL, *pidfile = NULL, **args = NULL;
    FILE *logfp = NULL;
    char *logslevel = NULL, *logfacility = NULL;
    int logilevel = NOTE;
    enum cmdaction action = SPAWN;
    struct opt opts;
    struct logging_syslog syslogopts;
    struct config *config;
    /* Parse arguments */
    arginit(&opts, argv);
    for (;;) {
        int opt = argparse(&opts);
        char *arg;
        if (opt == 0) {
            args = opts.argv + opts.curidx;
            break;
        } else if (opt == -1) {
            die("Internal error");
        } else if (opt == -2) {
            break;
        } else if (opt == -3) {
            arg = getarg(&opts, 0);
            if (strcmp(arg, "help") == 0) {
                usage(1, 0);
            } else if (strcmp(arg, "version") == 0) {
                puts(PROGNAME " " VERSION);
                return 0;
            } else if (strcmp(arg, "config") == 0) {
                conffile = getarg(&opts, 0);
                if (! conffile) {
                    fprintf(stderr, "Missing required argument for '--%s'\n",
                            arg);
                    usage(0, 2);
                }
            } else if (strcmp(arg, "log") == 0) {
                char *val = getarg(&opts, 0);
                if (! val) {
                    fprintf(stderr, "Missing required argument for '--%s'\n",
                            arg);
                    usage(0, 2);
                }
                if (strcasecmp(val, "stderr") == 0) {
                    logfp = stderr;
                } else {
                    logfacility = val;
                }
            } else if (strcmp(arg, "loglevel") == 0) {
                logslevel = getarg(&opts, 0);
                if (! logslevel) {
                    fprintf(stderr, "Missing required argument for '--%s'\n",
                            arg);
                    usage(0, 2);
                }
            } else if (strcmp(arg, "pid") == 0) {
                pidfile = getarg(&opts, 0);
                if (! pidfile) {
                    fprintf(stderr, "Missing required argument for '--%s'\n",
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
            char *eq;
            arg = getarg(&opts, 0);
            eq = strchr(arg, '=');
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
            case 'l':
                arg = getarg(&opts, 0);
                if (! arg) {
                    fprintf(stderr, "Missing required argument for '-%c'\n",
                            opt);
                    usage(0, 2);
                }
                if (strcasecmp(arg, "stderr") == 0) {
                    logfp = stderr;
                } else {
                    logfacility = arg;
                }
                break;
            case 'L':
                arg = getarg(&opts, 0);
                if (! arg) {
                    fprintf(stderr, "Missing required argument for '-%c'\n",
                            opt);
                    usage(0, 2);
                }
                logslevel = arg;
                break;
            case 'P':
                pidfile = getarg(&opts, 0);
                if (! pidfile) {
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
    /* Prepare logging */
    syslogopts.ident = PROGNAME;
    syslogopts.option = LOG_PID;
    syslogopts.facility = (logfacility) ? facility(logfacility) : -2;
    if (syslogopts.facility == -1) {
        fprintf(stderr, "Bad syslog facility: '%s'\n", logfacility);
        return 2;
    }
    if (logslevel) {
        logilevel = loglevel(logslevel);
        if (logilevel == -1) {
            fprintf(stderr, "Bad logging level: '%s'\n", logslevel);
            return 2;
        }
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
        /* Prepare logging */
        initlog(logfp, (syslogopts.facility == -2) ? NULL : &syslogopts,
                logilevel);
        ret = server_main(config, background, pidfile, args);
    } else {
        ret = client_main(config, action, args);
    }
    /* Clean up */
    config_free(config);
    /* Done */
    return ret;
}
