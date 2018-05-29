#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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
    unsigned char data[N];
    while (1) {
        ssize_t n = read(fd, data, N);
        if (n == -1) {
            perror("read");
            exit(1);
        }
        if (n == 0) {
            break;
        }
        for (size_t i = 0; i < (size_t)n;) {
            printf("%d %d\n", j++, data[i++]);
        }
    }
    close(fd);
    return 0;
}
