/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#define _GNU_SOURCE
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include "comm.h"

/* Size of ancillary data buffer */
#define ANCBUF_SIZE 256

/* Static function */
static int setup_addr(struct sockaddr_un *addr, struct config *conf);

/* Deallocate all the ressources associated with the given message */
void comm_del(struct ctlmsg *msg) {
    if (msg->fields && msg->fieldnum) {
        int i;
        for (i = 0; i < msg->fieldnum; i++) free(msg->fields[i]);
    }
    free(msg->fields);
    msg->fieldnum = 0;
    msg->fields = NULL;
    msg->creds.pid = -1;
    msg->creds.uid = -1;
    msg->creds.gid = -1;
    if (msg->fds[0] != -1) close(msg->fds[0]);
    if (msg->fds[1] != -1) close(msg->fds[1]);
    if (msg->fds[2] != -1) close(msg->fds[2]);
    msg->fds[0] = -1;
    msg->fds[1] = -1;
    msg->fds[2] = -1;
}

/* Free all ressources associated with the given message, and it itself */
void comm_free(struct ctlmsg *msg) {
    comm_del(msg);
    free(msg);
}

/* Set up the communication socket for listening for connections */
int comm_listen(struct config *conf) {
    struct sockaddr_un addr;
    int one = 1;
    /* Remove old path; ignore errors */
    unlink(conf->socketpath);
    /* Set up address */
    if (setup_addr(&addr, conf) == -1) return -1;
    /* Create socket */
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1)
        return -1;
    /* Change file permissions */
    if (fchmod(fd, 0777) == -1)
        goto error;
    /* Bind */
    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
        goto error;
    /* Allow credential receiving */
    if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &one, sizeof(one)) == -1)
        goto error;
    /* Replace old socket, if any */
    if (conf->socket != -1) close(conf->socket);
    conf->socket = fd;
    conf->flags |= CONFIG_UNLINK;
    /* Done */
    return fd;
    /* Something failed */
    error:
        close(fd);
        return -1;
}

/* Set up the communication socket for connecting to a "master" instance */
int comm_connect(struct config *conf) {
    struct sockaddr_un addr;
    int one = 1;
    /* Set up address */
    if (setup_addr(&addr, conf) == -1) return -1;
    /* Create socket */
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1)
        return -1;
    /* Connect */
    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1)
        goto error;
    /* Allow credential receiving */
    if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &one, sizeof(one)) == -1)
        goto error;
    /* Replace old socket */
    if (conf->socket != -1) close(conf->socket);
    conf->socket = fd;
    /* Done */
    return fd;
    /* Something failed */
    error:
        close(fd);
        return -1;
}

/* Receive a message from the communication socket */
int comm_recv(int fd, struct ctlmsg *msg, struct addr *addr, int flags) {
    char buf[MSG_MAXLEN], credbuf[ANCBUF_SIZE];
    int ret, i, j;
    struct addr raddr;
    struct iovec bufvec;
    struct msghdr hdr;
    struct cmsghdr *cmsg;
    /* Validate flags */
    if (flags & ~COMM_DONTWAIT) {
        errno = EINVAL;
        return -1;
    }
    /* Deallocate old data */
    comm_del(msg);
    /* Prepare buffers for receiving */
    bufvec.iov_base = buf;
    bufvec.iov_len = sizeof(buf);
    hdr.msg_name = &raddr.addr;
    hdr.msg_namelen = sizeof(raddr.addr);
    hdr.msg_iov = &bufvec;
    hdr.msg_iovlen = 1;
    hdr.msg_control = credbuf;
    hdr.msg_controllen = sizeof(credbuf);
    hdr.msg_flags = (flags & COMM_DONTWAIT) ? MSG_DONTWAIT : 0;
    /* Actually read message */
    ret = recvmsg(fd, &hdr, 0);
    if (ret == -1) {
        if (flags & COMM_DONTWAIT && (errno == EAGAIN ||
                                      errno == EWOULDBLOCK)) {
            return -2;
        }
        return -1;
    }
    raddr.addrlen = hdr.msg_namelen;
    /* Check for invalid messages */
    if (ret != 0 && buf[ret - 1]) {
        if (comm_senderr(fd, "BADMSG", "Bad message", &raddr, flags) == -1) {
            return -1;
        } else {
            return -2;
        }
    }
    /* Dissect contents */
    if (ret) {
        /* Determine field count */
        msg->fieldnum = 0;
        for (i = 0; i < ret; i++) if (! buf[i]) msg->fieldnum++;
        /* Allocate fields */
        msg->fields = calloc(msg->fieldnum, sizeof(char *));
        if (msg->fieldnum && ! msg->fields) {
            msg->fieldnum = 0;
            return -1;
        }
        /* Copy fields into structure */
        i = 0;
        j = 0;
        while (i < ret) {
            int l = strlen(buf + i) + 1;
            char *b = malloc(l);
            if (! b) goto memerr;
            memcpy(b, buf + i, l);
            msg->fields[j++] = b;
            i += l;
        }
    } else {
        msg->fieldnum = 0;
        msg->fields = NULL;
    }
    /* Retrieve ancillary data */
    for (cmsg = CMSG_FIRSTHDR(&hdr); cmsg; cmsg = CMSG_NXTHDR(&hdr, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET) continue;
        if (cmsg->cmsg_type == SCM_RIGHTS) {
            /* Obtain buffer and length */
            int *fds = (int *) CMSG_DATA(cmsg);
            int len = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            /* Close FD-s if incorrect amount passed or some are already
             * present */
            if (len != 3 || msg->fds[0] != -1) {
                while (len--) close(*fds++);
                continue;
            }
            /* Otherwise, copy into buffer */
            msg->fds[0] = fds[0];
            msg->fds[1] = fds[1];
            msg->fds[2] = fds[2];
        } else if (cmsg->cmsg_type == SCM_CREDENTIALS) {
            if (msg->creds.pid != -1) continue;
            struct ucred *creds = (struct ucred *) CMSG_DATA(cmsg);
            msg->creds = *creds;
        }
    }
    /* Fill in addr */
    if (addr) *addr = raddr;
    /* Done */
    return ret;
    /* Error while allocating payload */
    memerr:
        comm_del(msg);
        return -1;
}

/* Send a message through the communication socket */
int comm_send(int fd, struct ctlmsg *msg, struct addr *addr, int flags) {
    char buf[MSG_MAXLEN], credbuf[ANCBUF_SIZE];
    int buflen = 0, i, ret;
    struct iovec bufvec;
    struct msghdr hdr;
    struct cmsghdr *cmsg;
    /* Fill in process credentials */
    msg->creds.pid = getpid();
    msg->creds.uid = geteuid();
    msg->creds.gid = getegid();
    /* Validate flags */
    if (flags & ~COMM_DONTWAIT) {
        errno = EINVAL;
        return -1;
    }
    /* Assemble message into buffer */
    for (i = 0; i < msg->fieldnum; i++) {
        int l = strlen(msg->fields[i]) + 1;
        if (buflen + l > sizeof(buf)) {
            errno = E2BIG;
            return -1;
        }
        memcpy(buf + buflen, msg->fields[i], l);
        buflen += l;
    }
    /* Populate structures */
    bufvec.iov_base = buf;
    bufvec.iov_len = buflen;
    hdr.msg_name = (addr) ? &addr->addr : NULL;
    hdr.msg_namelen = (addr) ? addr->addrlen : 0;
    hdr.msg_iov = &bufvec;
    hdr.msg_iovlen = 1;
    hdr.msg_control = credbuf;
    hdr.msg_controllen = CMSG_SPACE(sizeof(struct ucred));
    hdr.msg_flags = (flags & COMM_DONTWAIT) ? MSG_DONTWAIT : 0;
    /* Populate ancillary messages */
    cmsg = CMSG_FIRSTHDR(&hdr);
    cmsg->cmsg_len = CMSG_LEN(sizeof(struct ucred));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_CREDENTIALS;
    memcpy(CMSG_DATA(cmsg), &msg->creds, sizeof(msg->creds));
    /* Add file descriptors if necessary */
    if (msg->fds[0] != -1 && msg->fds[1] != -1 && msg->fds[2] != -1) {
        hdr.msg_controllen += CMSG_SPACE(sizeof(msg->fds));
        cmsg = CMSG_NXTHDR(&hdr, cmsg);
        cmsg->cmsg_len = CMSG_LEN(sizeof(msg->fds));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        memcpy(CMSG_DATA(cmsg), msg->fds, sizeof(msg->fds));
    }
    /* Send message */
    ret = sendmsg(fd, &hdr, 0);
    if (ret == -1 && flags & COMM_DONTWAIT && (errno == EAGAIN ||
        errno == EWOULDBLOCK)) return -2;
    return ret;
}

/* Send an error message */
int comm_senderr(int fd, char *errcode, char *errmsg, struct addr *addr,
                 int flags) {
    /* Prepare message */
    char *fields[] = { "", errcode, errmsg };
    struct ctlmsg msg = CTLMSG_INIT;
    msg.fieldnum = 3;
    msg.fields = fields;
    /* Deliver message */
    return comm_send(fd, &msg, addr, flags);
}

/* Common address preparation */
static int setup_addr(struct sockaddr_un *addr, struct config *conf) {
    /* Verify path isn't too long */
    if (strlen(conf->socketpath) > sizeof(addr->sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    /* Set up structure */
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, conf->socketpath, sizeof(addr->sun_path));
    return 0;
}
