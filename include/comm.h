/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Communication
 * IPC is performed over UNIX domain datagram sockets. Lower-level message
 * boundaries are respected. Individual fields within a message are separated
 * by NUL bytes; the last field is additionally terminated by a NUL byte.
 * The first field of error messages is empty; the second field of those
 * contains a short mnemonic for the error, and the third one a user-readable
 * description. */

/* Requires _GNU_SOURCE. */

#ifndef _COMM_H
#define _COMM_H

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "config.h"

/* Maximum length of a control message */
#define MSG_MAXLEN 65536

/* Empty initializer for a struct ctlmsg */
#define CTLMSG_INIT { 0, NULL, { -1, -1, -1 }, { -1, -1, -1 } }

/* A communication message
 * Members:
 * fieldnum: (int) Amount of strings this message incorporates.
 * fields  : (char **) An array of NUL-terminated strings containing the
 *           individual parts of the message.
 * creds   : (struct ucred) The credentials of the process sending this
 *           message. Filled in by comm_send() and comm_recv().
 * fds     : (int [3]) File descriptors passed with the message, or all -1
 *           if none. */
struct ctlmsg {
    int fieldnum;
    char **fields;
    struct ucred creds;
    int fds[3];
};

/* A length-aware socket address
 * Members:
 * addr   : (struct sockaddr_un) The actual address.
 * addrlen: (socklen_t) The length of addr.
 */
struct addr {
    struct sockaddr_un addr;
    socklen_t addrlen;
};

/* Deallocate all the ressources associated with the given message
 * File descriptors (it present) are closed; reset them to -1 manually if
 * this is not desired.
 * Assumes all the arrays are dynamically allocated! */
void comm_del(struct ctlmsg *msg);

/* Free all ressources associated with the given message, and it itself
 * The remarks for comm_del() apply. */
void comm_free(struct ctlmsg *msg);

/* Set up the communication socket for listening for connections
 * Anything that was present at the path is removed; be cautious. This sets
 * the CONFIG_UNLINK flag on the given structure, to try to remove the socket
 * during cleanup.
 * Returns the file descriptor (which is also set in conf), or -1 on error
 * (setting errno, but not updating the structure member in that case). */
int comm_listen(struct config *conf);

/* Set up the communication socket for connecting to a "master" instance
 * See comm_listen() for return semantics. */
int comm_connect(struct config *conf);

/* Receive a message from the communication socket
 * The fields member of msg is set to a dynamically allocated array
 * (free()ing the former value as well as all fields referenced from there if
 * not NULL; this will fail with statically allocated buffers!), the
 * individual fields are dynamically allocated copies of the original data
 * received. If the messages contains a set of three file descriptors sent as
 * ancillary data, those are stored in the fds member of msg, otherwise, it
 * is filled with -1's.
 * If addr is not NULL, the address where the message originates from is
 * stored in there.
 * Returns the amount of bytes received (which may be zero), -2 if an invalid
 * message was received (having replied with an error messgae; it is the
 * caller's obligation to restart the call if desired), or -1 on error, with
 * errno set.
 * NOTE that a maximum length of MSG_MAXLEN is enforced; messages longer than
 *      that are rejected and an error message is sent. */
int comm_recv(int fd, struct ctlmsg *msg, struct addr *addr);

/* Send a message through the communication socket
 * The creds member of msg is filled in, regardless of whether the call fails
 * or not. If all elements of the fds member of msg are not -1, they are sent
 * along with the message.
 * addr (if not NULL) specifies the peer to send the message to.
 * Returns the amount of bytes sent, or -1 on error, with errno set.
 * NOTE that the maximum message length (of MSG_MAXLEN) is enforced as well;
 *      attempts to send longer messages are rejected with an E2BIG. Empty
 *      messages are rejected with a EINVAL. */
int comm_send(int fd, struct ctlmsg *msg, struct addr *addr);

/* Send an error message
 * The code and description are packed into an error message as described
 * above and sent over the given file descriptor.
 * See comm_send() for the semantics of addr.
 * Returns the amount of bytes send, or -1 on error, with errno set. */
int comm_senderr(int fd, char *errcode, char *errdesc, struct addr *addr);

#endif
