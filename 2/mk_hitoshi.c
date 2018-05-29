#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
    unsigned char data[] = {228, 186, 186, 229, 191, 151};
    int fd = open("hitoshi", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    size_t written = 0;
    while (written < sizeof(data)) {
        ssize_t written2 = write(fd, data + written, sizeof(data) - written);
        if (written2 == -1) {
            perror("write");
            exit(1);
        }
        written += (size_t)written2;
    }
    close(fd);
    return 0;
}
