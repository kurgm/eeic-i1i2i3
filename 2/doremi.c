#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int A, n;

short func(double f, double t);
void write_force(int fildes, const void *ptr, size_t nbyte);

short func(double f, double t) {
    return (short)(A * sin(2 * M_PI * f * t));
}

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
    if (argc != 3) {
        fprintf(stderr, "usage: %s A n\n", argv[0]);
        exit(1);
    }
    A = atoi(argv[1]);
    n = atoi(argv[2]);
    int diffs[] = {0, 2, 4, 5, 7, 9, 11};
    const int do_f = (int)(440.0 / 2.0 * pow(2.0, 3.0 * 1.0 / 12.0));
    for (int i = 0; i < n; i++) {
        double f = do_f * pow(2.0, i / 7 + diffs[i % 7] / 12.0);
        for (int j = 0; j < 13230; j++) {
            double t = j / 44100.0;
            short x = func(f, t);
            write_force(1, &x, sizeof(short));
        }
    }
    return 0;
}
