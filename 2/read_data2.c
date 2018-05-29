#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define N 256

int main(int argc, char **argv) {
    int fd;
    if (argc == 1) {
        fd = 0;
    } else if (argc != 2) {
        fprintf(stderr, "usage: %s filename\n", argv[0]);
        exit(1);
    } else {
        char *filename = argv[1];
        fd = open(filename, O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(1);
        }
    }
    int j = 0;
    short data[N];
    size_t rem = 0;
    while (1) {
        ssize_t nread = read(fd, (char *)data + rem, N * sizeof(short) - rem);
        if (nread == -1) {
            perror("read");
            exit(1);
        }
        size_t nbyte = rem + (size_t)nread;
        for (size_t i = 0; i < nbyte / sizeof(short);) {
            printf("%d %d\n", j++, data[i++]);
        }
        if (nread == 0) {
            break;
        }
        rem = nbyte % sizeof(short);
        if (rem != 0) {
            memmove(data, (char *)data + nbyte - rem, rem);
        }
    }
    close(fd);
    return 0;
}
