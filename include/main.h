/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Definitions for main.c */

#ifndef _MAIN_H
#define _MAIN_H

#include "config.h"

/* Program name and version */
#define PROGNAME "procmgr"
#define VERSION "v1.0"

/* Default configuration file location */
#define DEFAULT_CONFFILE "/etc/procmgr.cfg"

/* Action the main() routing can perform */
enum cmdaction { SPAWN, TEST, STOP, RELOAD };

/* Server main loop
 * background specifies whether to fork into background. argv is verified not
 * to contain anything; it is an error to have anything in it.
 * Returns 0 on success, or a positive integer on failure. */
int server_main(struct config *config, int background, char *argv[]);

/* Client main function
 * action is the action to perform, argv is an array of command-line
 * arguments to pass.
 * Returns 0 on success, or a positive integer on failure. */
int client_main(struct config *config, enum cmdaction action, char *argv[]);

#endif
