/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Online configuration representation
 * In contrast to the generic conffile.h, this stores configuration and
 * run-time data in an easily accessible format. To load these configuration
 * data, a subset of the file format as described in conffile.h is used; all
 * values described are optional, although leaving some out would be unwise.
 * The order of declarations does not matter (apart from global values, which
 * must be specified before any section not to be taken as part of that
 * section).
 *
 *     socket-path = <communication socket path>
 *     allow-uid = <default numerical UID to allow, or an explicit "none">
 *     allow-gid = <similar to allow-uid>
 *
 *     [prog-<name>]
 *     allow-uid = <default UID for all uid-* in this section>
 *     allow-gid = <default GID>
 *     cmd-<action> = <shell command to run for the given action>
 *     uid-<action> = <UID to allow to perform this action>
 *     gid-<action> = <GID to allow to perform this action>
 *     restart-delay = <seconds after which approximately to restart>
 *
 * Arbitrarily many program sections can be specified; out of same-named
 * ones, only the last is considered; similarly for all values. Spacing
 * between sections is purely decorational, although it increases legibility.
 * An example:
 *
 *     socket-path = /var/local/procmgr-local
 *     # If GID 99 is, say, wheel, and members of that group should be
 *     # allowed to perform actions from their own accounts.
 *     allow-gid = 99
 *
 *     # Interaction with this would happen via "procmgr game-server ..."
 *     [prog-game-server]
 *     # If John Doe is UID 1000.
 *     allow-uid = 1000
 *     # Note the exec, to ensure procmgr sees the UID of the server itself
 *     # and not of the shell.
 *     cmd-start = exec /home/johndoe/bin/game.server
 *     # Note the use of the $PID variable, which (following from above)
 *     # is the UID of the server.
 *     cmd-reload = kill -HUP $PID
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#include "conffile.h"
#include "jobs.h"

/* Default location of communication socket. */
#define SOCKET_PATH "/var/run/procmgr"

/* Unlink the socket path before closing */
#define CONFIG_UNLINK 1

/* The program should be running now. If it dies while this flag is true,
 * it is restarted. */
#define PROG_RUNNING 1
/* (Internal) The program is marked for removal. */
#define PROG_REMOVE 2

/* Shell to invoke actions with. */
#define ACTION_SHELL "/bin/sh"

struct program;
struct action;

/* Root configuration structure
 * Members:
 * socketpath: (char *) The filesystem path of the communication socket.
 *             Defaults to SOCKET_PATH.
 * socket    : (int) The (UNIX domain) socket to use for communications.
 *             Bound to by the daemon, connected to by the clients.
 * flags     : (int) Bitmask of CONFIG_* constants.
 * def_uid   : (int) The default value for allow_uid in actions.
 * def_gid   : (int) The default value for allow_gid in actions.
 * conffile  : (struct conffile *) The configuration file underlying this
 *             configuration. May be NULL.
 * jobs      : (struct jobqueue *) The queue of pending jobs.
 * programs  : (struct program *) A linked list of the programs configured
 *             and/or used. */
struct config {
    char *socketpath;
    int socket;
    int flags;
    int def_uid;
    int def_gid;
    struct conffile *conffile;
    struct jobqueue *jobs;
    struct program *programs;
};

/* Individual program
 * Members:
 * name       : (char *) Name of program.
 * pid        : (int) PID of the instance of the program currently running,
 *              or -1 if none.
 * flags      : (int) Flags. See the PROG_* constants for descriptions.
 * delay      : (int) Restart delay in seconds.
 * prev, next : (struct program *) Linked list interconnection.
 * act_start  : (struct action *) The action to start the program. If not
 *              configured, starting fails.
 * act_restart: (struct action *) The action to restart the program. If not
 *              configured, the program is stopped (if running) and started
 *              (again); if configured, the PID of the resulting process
 *              replaces the old recorded one.
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
 * The command is run by ACTION_SHELL, appended after a "-c" parameter;
 * additional positional arguments are passed after it. The execution
 * environment is empty, save for the following variables:
 * PATH=/bin:/usr/bin -- The path to get executables from. All other ones
 *                       must be fetched by absolute path.
 * SHELL              -- The shell used to run the command. Equal to the
 *                       ACTION_SHELL constant.
 * PROGNAME           -- The name of the current program.
 * ACTION             -- The name of the action being executed now.
 * PID                -- The PID of the process of the current program, or
 *                       the empty string if none.
 * The PID of the process that is running the "start" and "restart" actions
 * is recorded as the PID of the program as a whole; thus, the command for
 * these actions should preferably exec() the actual service to be run.
 * Members:
 * name     : (char *) The (statically allocated) name of this action.
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
    char *name;
    char *command;
    int allow_uid;
    int allow_gid;
};

/* Create a new runtime configuration based on the given configuration file
 * If file is NULL, the configuration is set to all defaults. If it is not,
 * the settings from the file are applied on top of that. Initially, no
 * programs are started.
 * If parsing fails, a message is written to stdout (unless quiet is false),
 * NULL is returned and errno is set as appropriately as possible. */
struct config *config_new(struct conffile *file, int quiet);

/* Deallocate all the underlying structures
 * Members that should not be deallocated must be extraced manually before
 * calling this. All others are properly disposed of and reset to placeholder
 * values. */
void config_del(struct config *conf);

/* Deallocate all the underlying structures and free this one
 * Equivalent to config_del(conf); free(conf);. */
void config_free(struct config *conf);

/* Re-read the underlying configuration file and merge the new configuration
 * with the current one
 * Programs that have vanished from the file and are not running are removed;
 * such ones that still are running persist until they are stopped; programs
 * whose configuration values have changed retain their runtime data; new
 * programs are added to the configuration, and not started.
 * Returns the amount of programs affected on success (removed ones count
 * positively), or -1 on fatal or -2 on non-fatal error with errno set,
 * having written a message to stderr first (if quiet is true). */
int config_update(struct config *conf, int quiet);

/* Add the given program to the configuration, merging the entries if
 * necessary */
void config_add(struct config *conf, struct program *prog);

/* Return the program named by the given string, or NULL if none */
struct program *config_get(struct config *conf, char *name);

/* Remove the given program from the configuration, deallocating it */
void config_remove(struct config *conf, struct program *prog);

/* Allocate a program using the configuration from the given configuration
 * object and configuration section (none, any, or both can be NULL) */
struct program *prog_new(struct config *conf, struct section *config);

/* Free all the resources underlying the given structure
 * If this is part of a linked list, the references are updated accordingly;
 * note however that the beginning of the list must be updated as well.
 * If any program corresponding to this section is running, nothing happens
 * to it. */
void prog_del(struct program *prog);

/* Deallocate the given structure, as well as any others linked to it */
void prog_free(struct program *prog);

/* Return the action named by name from prog, or NULL if none */
struct action *prog_action(struct program *prog, char *name);

#endif
