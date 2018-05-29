#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define N 256

static unsigned char data[N];

int main(int argc, char **argv) {
    for (int i = 0; i < N; i++) {
        data[i] = (unsigned char)i;
    }
    char *filename = "my_data";
    if (argc > 1) {
        filename = argv[1];
    }
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    size_t written = 0;
    while (written < N) {
        ssize_t written2 = write(fd, data + written, N - written);
        if (written2 == -1) {
            perror("write");
            exit(1);
        }
        written += (size_t)written2;
    }
    close(fd);
    return 0;
}
