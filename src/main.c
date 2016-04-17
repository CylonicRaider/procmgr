
/* Main program for configuration file parsing testing */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "conffile.h"

int main(int argc, char *argv[]) {
    struct conffile conf = { NULL, NULL };
    int curline, read;
    /* "Parse" command line */
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s filename\n", argv[0]);
        return 1;
    }
    /* Open file */
    conf.fp = fopen(argv[1], "r");
    if (conf.fp == NULL) {
        perror("fopen");
        return 1;
    }
    /* Parse configuration file */
    read = conffile_parse(&conf, &curline);
    if (read == -1) {
        char *errs = strerror(errno);
        fprintf(stderr, "Error on line %d: %s\n", curline, errs);
        return 1;
    }
    /* Display results */
    conffile_write(stdout, &conf);
    /* Done */
    return 0;
}
