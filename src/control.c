/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#define _GNU_SOURCE
#include <errno.h>
#include <unistd.h>

#include "control.h"

/* Static definitions */
static char *action_names[] = { "start", "restart", "reload", "signal",
    "stop", "status" };
#define action_count (sizeof(action_names) / sizeof(*action_names))
static int request_senderr(struct request *request, char *code, char *desc);

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
    /* TODO */
    errno = ENOSYS;
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
int run_jobs(struct config *config, int pid) {
    struct job *list, *next;
    int res, ret = 0;
    for (list = jobqueue_getfor(config->jobs, pid); list; list = next) {
        next = list->next;
        res = job_run(list);
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

/* Send an error message to the client as specified by the given request,
 * and return whether that succeeded. */
int request_senderr(struct request *request, char *code, char *desc) {
    return (comm_senderr(request->config->socket, code, desc,
                         &request->addr) != -1);
}
