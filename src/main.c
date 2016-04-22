
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

int main(int argc, char *argv[]) {
    FILE *fp;
    int fd, listen;
    struct conffile *conffile;
    struct config *config;
    struct ctlmsg msg = { 0, NULL, { -1, -1, -1 } };
    struct sockaddr_un addr;
    /* "Parse" command line */
    if (argc < 2 || argc > 3 || (argc == 3 &&
            strcmp(argv[2], "-l") != 0)) {
        fprintf(stderr, "USAGE: %s filename [-l]\n", argv[0]);
        return 1;
    }
    listen = (argc == 3);
    /* Open file */
    fp = fopen(argv[1], "r");
    if (fp == NULL) die("fopen");
    /* Create conffile */
    conffile = conffile_new(fp);
    if (conffile == NULL) die("conffile_new");
    /* Create config */
    config = config_new(conffile, 0);
    if (config == NULL) die("config_new");
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
            if (comm_recv(fd, &msg, &addr) == -1)
                die("comm_recv");
            if (comm_send(fd, &msg, &addr) == -1)
                die("comm_send");
        }
    } else {
        char *buffer = NULL, **parts = NULL, *p;
        size_t buflen = 0;
        int nparts, i;
        for (;;) {
            /* Read line */
            int res = readline(stdin, &buffer, &buflen);
            if (res == -1) die("readline");
            if (strlen(buffer) != res) {
                fprintf(stderr, "Embedded NUL found, aborting.\n");
                break;
            }
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
        }
        free(buffer);
    }
    /* Delete config */
    config_free(config);
    /* Done */
    return 0;
}
