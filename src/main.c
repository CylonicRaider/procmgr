
/* Main program for configuration parsing testing */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

void die(char *func) {
    perror(func);
    exit(1);
}

int main(int argc, char *argv[]) {
    FILE *fp;
    struct conffile *conffile;
    struct config *config;
    /* "Parse" command line */
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s filename\n", argv[0]);
        return 1;
    }
    /* Open file */
    fp = fopen(argv[1], "r");
    if (fp == NULL) die("fopen");
    /* Create conffile */
    fprintf(stderr, "Making conffile...\n");
    conffile = conffile_new(fp);
    if (conffile == NULL) die("conffile_new");
    /* Create config */
    fprintf(stderr, "Making config...\n");
    config = config_new(conffile, 0);
    /* Delete config */
    fprintf(stderr, "Deleting config...\n");
    config_free(config);
    /* Done */
    fprintf(stderr, "Done.\n");
    return 0;
}
