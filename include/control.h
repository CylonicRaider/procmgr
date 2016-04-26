/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Actual actions
 * This module implements executive actions crossing the boundaries of other
 * modules. */

#ifndef _CONTROL_H
#define _CONTROL_H

#include "comm.h"
#include "config.h"

/* get_reply() encountered an error */
#define REPLY_ERROR 65535

/* Obtain the program corresponding to the request
 * If addr is not NULL, an error message is sent there if there is no such
 * program or action.
 * Returns the program, or NULL if none. */
struct program *get_program(struct config *config, struct ctlmsg *msg,
                            struct addr *addr);

/* Obtain the action corresponding to the request
 * If addr is not NULL, an error message is sent there if there is no such
 * program or action.
 * Returns the action, or NULL if none. */
struct action *get_action(struct program *program, struct ctlmsg *msg,
                          struct addr *addr);

/* Verify that the implicitly given credentials are authorized to perform the
 * action
 * Authorized are either credentials with a UID of 0 (root), or such where
 * either the UID or the GID matches is not -1 and matches the respective
 * entry in the action.
 * If addr is not NULL, an error message is sent to it if authorization
 * fails.
 * Returns an integer greater than zero if authorization succeeds, zero if
 * it fails, or -1 if a fatal error happens. */
int validate_action(struct action *action, struct ctlmsg *msg,
                    struct addr *addr);

/* Perform the given action
 * The actual things to do are added to the job queue.
 * Returns zero on success, or -1 on failure. */
int schedule_action(struct config *config, struct program *prog,
                    struct action *action, struct ctlmsg *msg,
                    struct addr *addr);

/* Extract jobs matching the given PID from the queue and spawn them
 * Returns zero in case of success, or -1 on error. */
int run_jobs(struct config *config, int pid);

/* Send a request to perform an action as specified in the argument list
 * Invalid actions are filtered out. Returns zero on success or -1 on
 * error. */
int send_request(struct config *cfg, char **argv);

/* Wait for a message to arrive and return the desired return code
 * The return value is either the return code (if everything succeeds), which
 * can be in the range [-255..255], or the constant REPLY_ERROR if a fatal
 * error happened, with errno set properly. */
int get_reply(struct config *cfg);

#endif
