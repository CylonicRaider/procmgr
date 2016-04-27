
/* Main program for configuration parsing testing */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "readline.h"
#include "comm.h"

void die(char *func) {
    perror(func);
    exit(1);
}

void print_msg(FILE *fp, struct ctlmsg *msg) {
    int i;
    char *p;
    fputs("[msg [", fp);
    for (i = 0; i < msg->fieldnum; i++) {
        if (i) fputs(", ", fp);
        fputc('"', fp);
        for (p = msg->fields[i]; *p; p++) {
            if (*p == '\n') {
                fputs("\\n", fp);
                continue;
            }
            if (*p == '"' || *p == '\\') fputc('\\', fp);
            fputc(*p, fp);
        }
        fputc('"', fp);
    }
    fprintf(fp, "] {pid=%d uid=%d gid=%d} {in=%d out=%d err=%d}]\n",
            msg->creds.pid, msg->creds.uid, msg->creds.gid,
            msg->fds[0], msg->fds[1], msg->fds[2]);
}

int main(int argc, char *argv[]) {
    FILE *fp;
    int fd, listen;
    struct conffile *conffile;
    struct config *config;
    struct ctlmsg msg = CTLMSG_INIT;
    struct addr addr;
    /* "Parse" command line */
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "-l") != 0)) {
        fprintf(stderr, "USAGE: %s filename [-l]\n", argv[0]);
        return 1;
    }
    listen = (argc == 3);
    /* Open file */
    fp = fopen(argv[1], "r");
    if (! fp) die("fopen");
    /* Create conffile */
    conffile = conffile_new(fp);
    if (! conffile) die("conffile_new");
    /* Create config */
    config = config_new(conffile, 0);
    if (! config) die("config_new");
    /* Create socket */
    if (listen) {
        fd = comm_listen(config);
        if (fd == -1) die("comm_listen");
    } else {
        fd = comm_connect(config);
        if (fd == -1) die("comm_connect");
    }
    /* Main loop */
    if (listen) {
        for (;;) {
            int res = comm_recv(fd, &msg, &addr);
            if (res == -1) {
                die("comm_recv");
            } else if (res == -2) {
                continue;
            }
            print_msg(stdout, &msg);
            if (comm_send(fd, &msg, &addr) == -1)
                die("comm_send");
        }
        comm_del(&msg);
    } else {
        struct ctlmsg msg2 = CTLMSG_INIT;
        char *buffer = NULL, **parts = NULL, *p;
        size_t buflen = 0;
        int nparts, i;
        msg.fds[0] = 0;
        msg.fds[1] = 1;
        msg.fds[2] = 2;
        for (;;) {
            /* Read line */
            int res = readline(stdin, &buffer, &buflen);
            if (res == -1) die("readline");
            if (res == 0) break;
            if (strlen(buffer) != res) {
                fprintf(stderr, "Embedded NUL found, aborting.\n");
                break;
            }
            /* Strip newline */
            if (buffer[res - 1] == '\n')
                buffer[--res] = '\0';
            /* Allocate parts */
            nparts = 1;
            for (i = 0; i < res; i++) {
                if (buffer[i] != '\t') continue;
                nparts++;
                buffer[i] = '\0';
            }
            parts = realloc(parts, nparts * sizeof(char *));
            if (! parts) die("realloc");
            /* Split into those */
            p = buffer;
            for (i = 0; i < nparts; i++) {
                parts[i] = p;
                p += strlen(p) + 1;
            }
            /* Prepare message */
            msg.fieldnum = nparts;
            msg.fields = parts;
            /* Send message */
            if (comm_send(fd, &msg, NULL) == -1)
                die("comm_send");
            /* Receive reply */
            if (comm_recv(fd, &msg2, NULL) < 0)
                die("comm_recv");
            print_msg(stdout, &msg2);
        }
        free(buffer);
        msg.fieldnum = 0;
        msg.fields = NULL;
        memset(msg.fds, 0, sizeof(msg.fds));
        comm_del(&msg);
        comm_del(&msg2);
    }
    /* Delete config */
    config_free(config);
    /* Done */
    return 0;
}
