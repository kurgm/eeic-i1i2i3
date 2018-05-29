#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define N 256

static short buf[N];

void write_force(int fildes, const void *ptr, size_t nbyte);

void write_force(int fildes, const void *ptr, size_t nbyte) {
    size_t written = 0;
    while (written < nbyte) {
        ssize_t written2 =
            write(fildes, (const char *)ptr + written, nbyte - written);
        if (written2 == -1) {
            perror("write failed");
            exit(1);
        }
        written += (size_t)written2;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s down-rate\n", argv[0]);
        exit(1);
    }
    int rate = atoi(argv[1]);
    if (rate <= 0) {
        fputs("invalid rate\n", stderr);
        exit(1);
    }
    size_t i = 0;
    size_t rem = 0;
    while (1) {
        ssize_t nread = read(0, (char *)buf + rem, sizeof(buf) - rem);
        if (nread == -1) {
            perror("read");
            exit(1);
        }
        size_t nbyte = rem + (size_t)nread;
        for (; i < nbyte / sizeof(short); i += (unsigned)rate) {
            write_force(1, &buf[i], sizeof(short));
        }
        if (nread == 0) {
            break;
        }
        i -= nbyte / sizeof(short);
        rem = nbyte % sizeof(short);
        if (rem != 0) {
            memmove(buf, buf + nbyte / sizeof(short), rem);
        }
    }
    return 0;
}
