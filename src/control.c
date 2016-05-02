/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include "control.h"

/* Static definitions */
struct waiter {
    int fd;
    int pid;
    struct addr replyto;
};

static char *action_names[] = { "start", "restart", "reload", "signal",
    "stop", "status" };
#define action_count (sizeof(action_names) / sizeof(*action_names))
static int setup_fds(struct request *request);
static int request_senderr(struct request *request, char *code, char *desc);
static int request_reply(int fd, struct addr *addr, int code);
static struct job *submit_waiter(struct request *request, int pid);
static struct job *submit_job(struct request *request);
static int _run_waiter(void *data, int retcode);
static int _run_request(void *data, int retcode);
static void _free_request(void *data);
static char *concat(char *s1, char *s2);

/* Duplicate a file descriptor or no file descriptor, returning whether
 * successful */
static inline int dupfd(int from, int *to) {
    if (from == -1) {
        *to = -1;
        return 1;
    } else {
        *to = dup(from);
        return (*to != -1);
    }
}

/* Create a request from the given message */
struct request *request_new(struct config *config, struct ctlmsg *msg,
                            struct addr *addr) {
    int i;
    /* Allocate structure */
    struct request *ret = calloc(1, sizeof(struct request));
    if (ret == NULL) return NULL;
    ret->fds[0] = ret->fds[1] = ret->fds[2] = -1;
    /* Check field amount */
    if (msg->fieldnum < 3) {
        if (comm_senderr(config->socket, "NOPARAMS", "Missing parameters",
            addr) == -1) goto error;
        goto errmsg;
    }
    /* Fill in members */
    ret->config = config;
    ret->program = config_get(config, msg->fields[1]);
    if (! ret->program) {
        if (comm_senderr(config->socket, "NOPROG", "No such program",
            addr) == -1) goto error;
        goto errmsg;
    }
    ret->action = prog_action(ret->program, msg->fields[2]);
    if (! ret->action) {
        if (comm_senderr(config->socket, "NOACTION", "No such action",
            addr) == -1) goto error;
        goto errmsg;
    }
    ret->argv = calloc(msg->fieldnum - 2, sizeof(char *));
    if (! ret->argv) goto error;
    for (i = 3; i < msg->fieldnum; i++) {
        char *d = strdup(msg->fields[i]);
        if (! d) goto error;
        ret->argv[i - 3] = d;
    }
    ret->creds = msg->creds;
    ret->fds[0] = msg->fds[0];
    ret->fds[1] = msg->fds[1];
    ret->fds[2] = msg->fds[2];
    ret->addr = *addr;
    ret->reply = 1;
    /* Done */
    return ret;
    /* Error handling */
    errmsg:
        errno = 0;
    error:
        if (ret) request_free(ret);
        if (msg->fds[0] != -1) close(msg->fds[0]);
        if (msg->fds[1] != -1) close(msg->fds[1]);
        if (msg->fds[2] != -1) close(msg->fds[2]);
        return NULL;
}

/* Verify that the credentials given in the request are authorized to
 * perform the requested action */
int request_validate(struct request *request) {
    if (request->creds.uid == -1 || request->creds.gid == -1) {
        return (request_senderr(request, "BADAUTH",
                                "Not authorized")) ? 0 : -1;
    }
    if (request->creds.uid == 0) return 1;
    if (! (request->creds.uid == request->action->allow_uid ||
           request->creds.gid == request->action->allow_gid)) {
        return (request_senderr(request, "BADAUTH",
                                "Not authorized")) ? 0 : -1;
    }
    return 1;
}

/* Perform the given action */
int request_run(struct request *request) {
    struct program *prog = request->program;
    struct request *req = NULL;
    struct job *job;
    int ret = 0;
    /* Update flags */
    if (request->action == prog->act_start ||
            request->action == prog->act_restart) {
        prog->flags |= PROG_RUNNING;
    } else if (request->action == prog->act_stop) {
        prog->flags &= ~PROG_RUNNING;
    }
    /* Do something */
    if (! request->action->command) {
        /* Perform default actions */
        if (request->action == prog->act_start) {
            /* Cannot really do anything */
            if (request_senderr(request, "NOCMD", "Cannot start"))
                errno = 0;
            return -1;
        } else if (request->action == prog->act_restart) {
            int res;
            /* Clone request */
            struct request *req = calloc(1, sizeof(struct request));
            if (! dupfd(request->fds[0], &req->fds[0])) goto error;
            if (! dupfd(request->fds[1], &req->fds[1])) goto error;
            if (! dupfd(request->fds[2], &req->fds[2])) goto error;
            req->config = request->config;
            req->program = prog;
            req->action = prog->act_start;
            req->argv = request->argv;
            req->creds = request->creds;
            req->addr = request->addr;
            /* "exec" different action using this request */
            request->action = prog->act_stop;
            request->argv = NULL;
            request->reply = 0;
            res = request_run(request);
            if (res == -1) goto error;
            /* Dispatch follow-up action */
            job = submit_job(req);
            if (! job) goto error;
            if (res) job->waitfor = res;
            return 0;
        } else if (request->action == prog->act_reload) {
            /* Restart program */
            request->action = prog->act_restart;
            return request_run(request);
        } else if (request->action == prog->act_signal) {
            /* Do nothing */
            if (! request->reply) return 0;
            return (request_reply(request->config->socket, &request->addr,
                                  0)) ? 0 : -1;
        } else if (request->action == prog->act_stop) {
            /* Kill process, if any
             * Reply will be sent when it dies */
            if (prog->pid != -1) {
                return (kill(prog->pid, SIGTERM) == -1) ? -1 : 0;
            }
            return 0;
        } else if (request->action == prog->act_status) {
            /* Write "running" or "not running" depending on whether there is
             * a process or not. */
            ret = fork();
            if (ret == 0) {
                setup_fds(request);
                if (prog->pid != -1) {
                    printf("running\n");
                    fflush(stdout);
                    _exit(0);
                } else {
                    printf("not running\n");
                    fflush(stdout);
                    _exit(1);
                }
            }
        } else {
            /* Should not happen at this point */
            errno = EFAULT;
            return -1;
        }
    } else {
        char **p, **argv, *envp[5], pidbuf[64];
        /* Prepare for job spawning */
        int l = 3;
        for (p = request->argv; *p; p++) l++;
        argv = calloc(l, sizeof(char *));
        if (! argv) return -1;
        memcpy(argv + 3, request->argv, (l - 3) * sizeof(char *));
        argv[0] = ACTION_SHELL;
        argv[1] = "-c";
        argv[2] = request->action->command;
        /* Prepare environment */
        envp[0] = concat("PATH=", ACTION_PATH);
        envp[1] = concat("SHELL=", ACTION_SHELL);
        envp[2] = concat("PROGNAME=", prog->name);
        envp[3] = concat("ACTION=", request->action->name);
        if (prog->pid == -1) {
            envp[4] = "PID=";
        } else {
            snprintf(pidbuf, sizeof(pidbuf), "PID=%d", prog->pid);
            envp[4] = pidbuf;
        }
        /* Spawn child process */
        ret = fork();
        if (ret == 0) {
            /* In child: configure file descriptors */
            setup_fds(request);
            /* exec() script */
            execve(argv[0], argv, envp);
            _exit(127);
        }
        /* Clean up */
        for (l = 0; l < 4; l++) free(envp[l]);
    }
    /* Only falling through here if we want to wait on something ->
     * Schedule waiter */
    if (ret != -1 && request->reply) {
        int stopping = (request->action == prog->action_stop);
        int waitfor = (stopping) ? prog->pid : ret;
        if (! submit_waiter(request, waitfor))
            return -1;
    }
    return ret;
    /* Someting failed */
    error:
        if (req) request_free(req);
        return -1;
}

/* Deallocate the given request after de-initializing it */
void request_free(struct request *request) {
    if (request->argv) {
        char **p;
        for (p = request->argv; *p; p++) free(*p);
    }
    free(request->argv);
    if (request->fds[0] != -1) close(request->fds[0]);
    if (request->fds[1] != -1) close(request->fds[1]);
    if (request->fds[2] != -1) close(request->fds[2]);
    free(request);
}

/* Extract jobs matching the given PID from the queue and run them */
int run_jobs(struct config *config, int pid, int retcode) {
    struct job *list, *next;
    int res, ret = 0;
    for (list = jobqueue_getfor(config->jobs, pid); list; list = next) {
        next = list->next;
        res = job_run(list, retcode);
        if (res == -1) {
            job_free(list);
            return -1;
        } else if (res == 0) {
            res = -1;
        }
        if (list->successor) {
            list->successor->waitfor = res;
            jobqueue_prepend(config->jobs, list->successor);
            list->successor = NULL;
        }
        job_del(list);
        free(list);
        ret++;
    }
    return ret;
}

/* Send a request to perform an action as specified in the argument list */
int send_request(struct config *config, char **argv) {
    struct ctlmsg msg = CTLMSG_INIT;
    int i, ret;
    char **p;
    /* Calculate field amount */
    msg.fieldnum = 1;
    for (p = argv; *p; p++) msg.fieldnum++;
    /* Validate request */
    if (msg.fieldnum < 3) return 0;
    for (i = 0; i < action_count; i++) {
        if (strcmp(argv[1], action_names[i]) == 0) break;
    }
    if (i == action_count) return 0;
    /* Allocate data */
    msg.fields = calloc(msg.fieldnum, sizeof(char *));
    if (! msg.fields) return -1;
    msg.fields[0] = "RUN";
    for (i = 1; i < msg.fieldnum; i++) {
        msg.fields[i] = argv[i - 1];
    }
    /* Fill in fields */
    msg.fds[0] = STDIN_FILENO;
    msg.fds[1] = STDOUT_FILENO;
    msg.fds[2] = STDERR_FILENO;
    /* Send! */
    ret = comm_send(config->socket, &msg, NULL);
    /* Clean up */
    free(msg.fields);
    /* Done */
    return ret;
}

/* Wait for a message to arrive and return the desired return code */
int get_reply(struct config *config) {
    struct ctlmsg msg = CTLMSG_INIT;
    int ret;
    char *end;
    /* Receive! */
    ret = comm_recv(config->socket, &msg, NULL);
    /* Abort on error. */
    if (ret < 0) {
        ret = REPLY_ERROR;
        goto end;
    }
    /* Check if the reply is most basically valid. */
    if (msg.fieldnum < 2) {
        fprintf(stderr, "ERROR: Bad message received\n");
        goto error;
    }
    /* Check for error messages */
    if (! *msg.fields[0]) {
        if (msg.fieldnum < 3) {
            fprintf(stderr, "ERROR: Bad error message received\n");
            goto error;
        }
        fprintf(stderr, "ERROR: (%s) %s\n", msg.fields[1], msg.fields[2]);
        goto error;
    }
    /* Obtain return value */
    errno = 0;
    ret = strtol(msg.fields[1], &end, 0);
    if (errno || *end) {
        fprintf(stderr, "ERROR: Invalid number in message\n");
        goto error;
    }
    /* Check if it is in range. */
    if (ret <= -256 || ret >= 256) {
        fprintf(stderr, "ERROR: Number out of bounds\n");
        goto error;
    }
    goto end;
    /* Done. */
    error:
        ret = REPLY_ERROR;
        errno = 0;
    end:
        comm_del(&msg);
        return ret;
}

int close_from(int minfd) {
    int i, dfd, fd, ret = -1;
    DIR *dir;
    char path[PATH_MAX], *end;
    /* Close all below 1024 */
    for (i = minfd; i < 1024; i++) close(i);
    /* Detect remaining FD-s from /proc */
    snprintf(path, sizeof(path), "/proc/%d/fd/", getpid());
    dir = opendir(path);
    if (! dir) return -1;
    dfd = dirfd(dir);
    for (;;) {
        struct dirent *ent;
        errno = 0;
        ent = readdir(dir);
        if (! ent && errno) goto end;
        errno = 0;
        fd = strtol(ent->d_name, &end, 10);
        if (*end || errno) {
            errno = EINVAL;
            goto end;
        }
        if (fd < minfd || fd == dfd) continue;
        close(fd);
    }
    ret = 0;
    end:
        closedir(dir);
        return ret;
}

/* Set up file descriptors for running as a child */
int setup_fds(struct request *request) {
    /* Override stdio with those in the request */
    if (request->fds[0] != -1 && dup2(request->fds[0], 0) == -1) return -1;
    if (request->fds[1] != -1 && dup2(request->fds[1], 1) == -1) return -1;
    if (request->fds[2] != -1 && dup2(request->fds[2], 2) == -1) return -1;
    if (request->fds[0] == -1) close(0);
    if (request->fds[1] == -1) close(1);
    if (request->fds[2] == -1) close(2);
    close_from(3);
}

/* Send an error message to the client as specified by the given request,
 * and return whether that succeeded. */
int request_senderr(struct request *request, char *code, char *desc) {
    return (comm_senderr(request->config->socket, code, desc,
                         &request->addr) != -1);
}

/* Send a successful completion message */
int request_reply(int fd, struct addr *addr, int code) {
    char numbuf[64], *fields[] = { "OK", numbuf };
    struct ctlmsg msg = CTLMSG_INIT;
    snprintf(numbuf, sizeof(numbuf), "%d", code);
    msg.fields = fields;
    msg.fieldnum = sizeof(fields) / sizeof(*fields);
    return (comm_send(fd, &msg, addr) != -1);
}

/* Schedule a job wrapping this request to be run */
struct job *submit_job(struct request *request) {
    struct job *ret = job_new(_run_request, _free_request, request);
    if (! ret) return NULL;
    jobqueue_append(request->config->jobs, ret);
    return ret;
}

/* Schedule a job wrapping this request to be run */
struct job *submit_waiter(struct request *request, int pid) {
    struct waiter *wt;
    struct job *ret;
    wt = malloc(sizeof(struct waiter));
    if (! wt) return NULL;
    wt->fd = request->config->socket;
    wt->pid = pid;
    wt->replyto = request->addr;
    ret = job_new(_run_waiter, free, wt);
    if (! ret) {
        free(wt);
        return NULL;
    }
    ret->waitfor = pid;
    jobqueue_append(request->config->jobs, ret);
    return ret;
}

/* Notify the process blocking on this waiter */
int _run_waiter(void *data, int retcode) {
    struct waiter *wt = data;
    if (retcode == JOB_NOEXIT) {
        errno = EINVAL;
        return -1;
    }
    return (request_reply(wt->fd, &wt->replyto, retcode)) ? 0 : -1;
}

/* Execute the payload of the given request */
int _run_request(void *data, int retcode) {
    return request_run(data);
}

/* Deallocate the given request */
void _free_request(void *data) {
    request_free(data);
}

/* Concatenate two strings */
char *concat(char *s1, char *s2) {
    int l1 = strlen(s1), l2 = strlen(s2);
    char *r = malloc(l1 + l2 + 1);
    if (! r) return NULL;
    memcpy(r, s1, l1);
    memcpy(r + l1, s2, l2);
    r[l1 + l2] = '\0';
    return r;
}
