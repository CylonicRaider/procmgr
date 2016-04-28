/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#define _GNU_SOURCE
#include <errno.h>
#include <unistd.h>

#include "control.h"

char *action_names[] = { "start", "restart", "reload", "signal", "stop",
    "status" };
#define action_count (sizeof(action_names) / sizeof(*action_names))

/* Obtain the program corresponding to the request */
struct program *get_program(struct config *config, struct ctlmsg *msg) {
    if (msg->fieldnum < 1) return NULL;
    return config_get(config, msg->fields[0]);
}

/* Obtain the action corresponding to the request */
struct action *get_action(struct program *program, struct ctlmsg *msg) {
    if (msg->fieldnum < 2) return NULL;
    return prog_action(program, msg->fields[1]);
}

/* Verify that the implicitly given credentials are authorized to perform the
 * action */
int validate_action(struct action *action, struct ctlmsg *msg) {
    if (msg->creds.uid == -1 || msg->creds.gid == -1) return 0;
    if (msg->creds.uid == 0) return 1;
    return (msg->creds.uid == action->allow_uid ||
            msg->creds.gid == action->allow_gid);
}

/* Perform the given action */
int schedule_action(struct config *config, struct program *prog,
                    struct action *action, struct ctlmsg *msg,
                    struct addr *addr) {
    /* NYI */
    errno = ENOSYS;
    return -1;
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
    char **p;
    int i;
    /* Calculate field amount */
    for (p = argv; *p; p++) msg.fieldnum++;
    /* Validate request */
    if (msg.fieldnum < 2) return 0;
    for (i = 0; i < action_count; i++) {
        if (strcmp(argv[1], action_names[i]) == 0) break;
    }
    if (i == action_count) return 0;
    /* Fill in fields */
    msg.fields = argv;
    msg.fds[0] = STDIN_FILENO;
    msg.fds[1] = STDOUT_FILENO;
    msg.fds[2] = STDERR_FILENO;
    /* Send! */
    return comm_send(config->socket, &msg, NULL);
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
