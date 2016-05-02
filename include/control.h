/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Actual actions
 * This module implements executive actions crossing the boundaries of other
 * modules. */

/* Includes comm.h, and thus requires _GNU_SOURCE. */

#ifndef _CONTROL_H
#define _CONTROL_H

#include "comm.h"
#include "config.h"

/* get_reply() encountered an error */
#define REPLY_ERROR 65535

/* Server-side representation of a request, as populated and acted upon
 * by the functions in here.
 * Members:
 * config : (struct config *) The configuration this request is related to.
 * program: (struct program *) The program this request relates to.
 * action : (struct action *) The action to perform.
 * argv   : (char **) Auxillary command-line arguments for the action.
 *          The only dynamically allocated member of the structure; may
 *          be NULL.
 * creds  : (struct ucred) Credentials of the process to submit the request.
 * fds    : (int [3]) A set of file descriptors to pass to the script.
 * addr   : (struct addr) The address to send replies to.
 * reply  : (int) Whether this request should be replied to. Mostly for
 *          internal use; defaults to 1. */
struct request {
    struct config *config;
    struct program *program;
    struct action *action;
    char **argv;
    struct ucred creds;
    int fds[3];
    struct addr addr;
    int reply;
};

/* Create a request from the given message
 * It is assumed that the message was verified to contain an appropriate
 * command.
 * If the request is incomplete, an error message is sent to addr, and
 * an error of 0 ("Success") is raised.
 * Returns a pointer to the request structure, or NULL on failure. */
struct request *request_new(struct config *config, struct ctlmsg *msg,
                            struct addr *addr);

/* Verify that the credentials given in the request are authorized to
 * perform the requested action
 * Authorized are credentials where both the UID and GID are not -1; in
 * addition, either the UID must be 0, or the UID or GID must match the
 * corresponding entry in the action.
 * Returns an integer greater than zero if authorization succeeds, zero if
 * it fails (an error message is sent to the client as specified by the
 * request), or -1 if a fatal error happens. */
int request_validate(struct request *request);

/* Perform the given action
 * Might perform immediately, or submit a job to the configuration's queue.
 * Returns the PID of the process spawned (if any), 0 if none, or -1 on
 * error, with errno either set to 0 ("Success") on a non-fatal error, or
 * otherwise on a fatal one. */
int request_run(struct request *request);

/* Deallocate the given request after de-initializing it
 * All associated ressources are freed, including the file descriptors, which
 * are closed. */
void request_free(struct request *request);

/* Extract jobs matching the given PID from the queue and spawn them
 * pid may be -1, in that case jobs which do not wait on a particular PID
 * are run. retcode is passed through to the job_run().
 * Returns the amount of jobs run in case of success, or -1 on error. */
int run_jobs(struct config *config, int pid, int retcode);

/* Send a request to perform an action as specified in the argument list
 * argv is expected to contain the actual values to send, consisting of a
 * protocol-level command and of its parameters.
 * Invalid actions are filtered out. Returns a positive number on success,
 * zero if the argument list is invalid (most notably by being too short,
 * too long, or by containing an invalid action), or -1 on fatal error. */
int send_request(struct config *config, char **argv);

/* Wait for a message to arrive and return the desired return code
 * The return value is either the return code (if everything succeeds), which
 * can be in the range [-255..255], or the constant REPLY_ERROR if a fatal
 * error happened, with errno set properly. In the latter case, errno may
 * by zero ("Success"), indicating that an error message was received and
 * written to standard error. */
int get_reply(struct config *config);

/* Close all file descriptors not less than minfd */
int close_from(int minfd);

#endif
