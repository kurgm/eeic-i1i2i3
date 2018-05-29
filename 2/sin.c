#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int A, f, n;
short func(double t);
void write_force(int fildes, const void *ptr, size_t nbyte);

short func(double t) { return (short)(A * sin(2 * M_PI * f * t)); }

void write_force(int fildes, const void *ptr, size_t nbyte) {
    size_t written = 0;
     while (written < nbyte) {
        ssize_t written2 = write(fildes, (const char *)ptr + written, nbyte - written);
        if (written2 == -1) {
            perror("write failed");
            exit(1);
        }
        written += (size_t)written2;
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s A f n\n", argv[0]);
        exit(1);
    }
    A = atoi(argv[1]);
    f = atoi(argv[2]);
    n = atoi(argv[3]);
    for (int i = 0; i < n; i++) {
        double t = i / 44100.0;
        short x = func(t);
        write_force(1, &x, sizeof(short));
    }
}
