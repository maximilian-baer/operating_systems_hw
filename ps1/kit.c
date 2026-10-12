#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>

char *outname = "stdout";

void fatal(const char *action, const char *name, const char *purpose) {
    if(errno) {
        fprintf(stderr, "kit: Cannot %s %s for %s: %s\n", 
            action, name, purpose, strerror(errno));
    }else {
        fprintf(stderr, "kit: Cannot %s %s for %s\n", action, name, purpose);
    }
    
    exit(-1);
}

void copy_out (int fdin, int fdout, char *name) {
    char buf[4096];
    int n, w, off;

    while ((n = read(fdin, buf, sizeof(buf))) > 0) {
        off = 0;
        while (off < n){
            if((w = write(fdout, buf + off, n - off)) < 0) {
                fatal("write", outname, "output");
            } 
            off += w;
        }
        
    }
    if(n < 0) {
        fatal("read", name, "input");
    }
}


int main (int argc, char *argv[]) {
    int fdo = STDOUT_FILENO;
    int c;
    char *outfile = NULL;

    while ((c = getopt(argc, argv, "o:")) != -1) {
        if(c == 'o') {
            outfile = optarg;
        }else {
            fatal("parse", argv[optind - 1], "options");
        }
    }

    if (outfile) {
        fdo = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fdo < 0) {
            fatal("open", outfile, "writing");
        }
        outname = outfile;

    }
    if (optind == argc) {
        copy_out(STDIN_FILENO, fdo, "stdin");
    }else {
        for(int i = optind; i < argc; i++) {
            if(!strcmp(argv[i], "-")) {
                copy_out(STDIN_FILENO, fdo, "stdin");
            } else {
                int fdi = open(argv[i], O_RDONLY);
                if (fdi < 0) {
                    fatal("open", argv[i], "reading");
                }
                copy_out(fdi, fdo, argv[i]);
                if(close(fdi) < 0) {
                    fatal("close", argv[i], "reading");
                }
            }
        }
    }
    if(fdo != STDOUT_FILENO && close(fdo) < 0) {
        fatal("close", outfile, "writing");
    }

    return 0;
}