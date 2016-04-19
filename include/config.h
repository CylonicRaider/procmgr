/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Online configuration representation
 * In contrast to the generic conffile.h, this stores configuration and
 * run-time data in an easily accessible format. */

#ifndef _CONFIG_H
#define _CONFIG_H

/* The program should be running now. If it dies while this flag is true,
 * if will be restarted. */
#define PROG_RUNNING 1

/* Shell to invoke actions with. */
#define ACTION_SHELL "/bin/sh"

struct program;
struct action;

/* Root configuration structure
 * Members:
 * socketpath: (char *) The filesystem path of the communication socket.
 * socket    : (int) The (UNIX domain) socket to use for communications.
 *             Bound to by the daemon, connected to by the clients.
 * programs  : (struct program *) A linked list of the programs configured
 *             and/or used. */
struct config {
    char *socketpath;
    int socket;
    struct program *programs;
};

/* Individual program
 * Members:
 * name       : (char *) Name of program.
 * pid        : (int) PID of the instance of the program currently running,
 *              or -1 if none.
 * flags      : (int) Flags. See the PROG_* constants for descriptions.
 * delay      : (int) Restart delay in seconds. Reset to one after a
 *              successful program start, and increased exponentially (by two
 *              times) after each failed automatic restart.
 * prev, next : (struct program *) Linked list interconnection.
 * act_start  : (struct action *) The action to start the program. If not
 *              configured, starting fails.
 * act_restart: (struct action *) The action to restart the program. If not
 *              configured, the program is stopped and started
 * act_reload : (struct action *) The action to reload configuration. If not
 *              configured, the program is restarted (using act_restart).
 * act_signal : (struct action *) An arbitrary user-defined action. The
 *              default is to do nothing.
 * act_stop   : (struct action *) The action to stop the program. If not
 *              configured, the "main" process is killed using SIGTERM.
 * act_status : (struct action *) The action to check program status. If not
 *              configured, "running" is printed to stdout while checking
 *              status and 0 is returned if the program is running;
 *              otherwise, "not running" is printed, and 1 is returned. */
struct program {
    char *name;
    int pid;
    int flags;
    int delay;
    struct program *prev, *next;
    struct action *act_start;
    struct action *act_restart;
    struct action *act_reload;
    struct action *act_signal;
    struct action *act_stop;
    struct action *act_status;
};

/* Possible action to be performed on a program
 * The command is run by ACTION_SHELL (/bin/sh), appended after a "-c"
 * parameter; additional positional arguments are passed after it. The
 * execution environment is empty, save for the following variables:
 * PATH=/bin:/usr/bin -- The path to get executables from.
 * PROGNAME           -- The name of the current program.
 * ACTION             -- The name of the action being executed now.
 * PID                -- The PID of the process of the current program, or
 *                       the empty string if none.
 * Members:
 * command  : (char *) Shell command to be invoked when the action is
 *            requested.
 * allow_uid: (int) UID to allow to perform this action. For the action to
 *            be allowed, either the UID or the GID must match the EUID or
 *            EGID of the caller, respectively, or the caller must have an
 *            EUID of 0 (i.e., be root).
 * allow_gid: (int) GID to allow to perform this action, with semantics
 *            similar to allow_uid (except for the EUID 0 clause).
 */
struct action {
    char *command;
    int allow_uid;
    int allow_gid;
};

#endif
