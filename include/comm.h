/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Communication
 * IPC is performed over UNIX domain datagram sockets. Lower-level message
 * boundaries are respected. Individual fields within a message are separated
 * by NUL bytes; the last field is additionally terminated by a NUL byte.
 * The first field of error messages is empty; the second field of those
 * contains a short mnemonic for the error, and the third one a user-readable
 * description. */

/* Requires _GNU_SOURCE .*/

#ifndef _CONTROL_H
#define _CONTROL_H

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "config.h"

/* Maximum length of a control message */
#define MSG_MAXLEN 65536

/* A communication message
 * Members:
 * fieldnum: (int) Amount of strings this message incorporates.
 * fields  : (char **) An array of NUL-terminated strings containing the
 *           individual parts of the message.
 * creds   : (struct ucred) The credentials of the process sending this
 *           message. Filled in by comm_send() and comm_recv(). */
struct ctlmsg {
    int fieldnum;
    char **fields;
    struct ucred creds;
};

/* Deallocate all the ressources associated with the given message
 * Assumes all the arrays are dynamically allocated! */
void comm_del(struct ctlmsg *msg);

/* Free all ressources associated with the given message, and it itself */
void comm_free(struct ctlmsg *msg);

/* Set up the communication socket for listening for connections
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
 * received.
 * If addr is not NULL, the address where the message originates from is
 * stored in there.
 * Returns the amount of bytes received, zero if an invalid message was
 * received (having replied with an error messgae; it is the caller's
 * obligation to restart the call if desired), or -1 on error, with errno
 * set.
 * NOTE that a maximum length of MSG_MAXLEN is enforced; messages longer than
 *      that are rejected and an error message is sent. */
int comm_recv(int fd, struct ctlmsg *msg, struct sockaddr_un *addr);

/* Send a message through the communication socket
 * The creds member of msg is filled in, regardless of whether the call fails
 * or not.
 * addr (if not NULL) specifies the peer to send the message to.
 * Returns the amount of bytes sent, or -1 on error, with errno set.
 * NOTE that the maximum message length (of MSG_MAXLEN) is enforced as well;
 *      attempts to send longer messages are rejected with an E2BIG. Empty
 *      messages are rejected with a EINVAL. */
int comm_send(int fd, struct ctlmsg *msg, struct sockaddr_un *addr);

/* Send an error message
 * The code and description are packed into an error message as described
 * above and sent over the given file descriptor.
 * See comm_send() for the semantics of addr.
 * Returns the amount of bytes send, or -1 on error, with errno set. */
int comm_senderr(int fd, char *errcode, char *errdesc,
                 struct sockaddr_un *addr);

#endif
