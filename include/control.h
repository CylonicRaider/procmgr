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

/* Obtain the program corresponding to the request
 * Returns the program, or NULL if none. The only known mode of failure for
 * this are segmentation faults, so errors are not indicated specially. */
struct program *get_program(struct config *config, struct ctlmsg *msg);

/* Obtain the action corresponding to the request
 * Returns the action, or NULL if none; also see the return value remarks of
 * get_program(). */
struct action *get_action(struct program *program, struct ctlmsg *msg);

/* Verify that the implicitly given credentials are authorized to perform the
 * action
 * Authorized are credentials where both the UID and GID are not -1; in
 * addition, either the UID must be 0, or the UID or GID must match the
 * corresponding entry in the action.
 * Returns an integer greater than zero if authorization succeeds, zero if
 * it fails, or -1 if a fatal error happens. */
int validate_action(struct action *action, struct ctlmsg *msg);

/* Perform the given action
 * The actual things to do are added to the job queue.
 * Returns zero on success, or -1 on failure. */
int schedule_action(struct config *config, struct program *prog,
                    struct action *action, struct ctlmsg *msg,
                    struct addr *addr);

/* Extract jobs matching the given PID from the queue and spawn them
 * pid may be -1, in that case jobs which do not wait on a particular PID
 * are run.
 * Returns the amount of jobs run in case of success, or -1 on error. */
int run_jobs(struct config *config, int pid);

/* Send a request to perform an action as specified in the argument list
 * argv is expected to contain the actual "arguments" (i.e., such not
 * containing the program name) as a NULL-terminated array.
 * Invalid actions are filtered out. Returns a positive number on success,
 * zero if the argument list is invalid (most notably by being too small or
 * by containing an invalid action), or -1 on fatal error. */
int send_request(struct config *config, char **argv);

/* Wait for a message to arrive and return the desired return code
 * The return value is either the return code (if everything succeeds), which
 * can be in the range [-255..255], or the constant REPLY_ERROR if a fatal
 * error happened, with errno set properly. In the latter case, errno may
 * by zero ("Success"), indicating that an error message was received and
 * written to standard error. */
int get_reply(struct config *config);

#endif
