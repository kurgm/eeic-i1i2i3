#include <assert.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef short sample_t;

void die(char *s) __attribute__ ((noreturn));
ssize_t read_n(int fd, ssize_t n, void *buf);
ssize_t write_n(int fd, ssize_t n, void *buf);
void sample_to_complex(sample_t *s, complex double *X, long n);
void complex_to_sample(complex double *X, sample_t *s, long n);
void fft_r(complex double *x, complex double *y, long n, complex double w);
void fft(complex double *x, complex double *y, long n);
void ifft(complex double *y, complex double *x, long n);
int pow2check(long N);
void print_complex(FILE *wp, complex double *Y, long n);
void bandpass(complex double *x, long n, double f1, double f2);

void die(char *s) {
    perror(s);
    exit(1);
}

/* fd から 必ず n バイト読み, bufへ書く.
   n バイト未満でEOFに達したら, 残りは0で埋める.
   fd から読み出されたバイト数を返す */
ssize_t read_n(int fd, ssize_t n, void *buf) {
    ssize_t re = 0;
    while (re < n) {
        ssize_t r = read(fd, (char *)buf + re, (size_t)(n - re));
        if (r == -1) die("read");
        if (r == 0) break;
        re += r;
    }
    memset((char *)buf + re, 0, n - re);
    return re;
}

/* fdへ, bufからnバイト書く */
ssize_t write_n(int fd, ssize_t n, void *buf) {
    ssize_t wr = 0;
    while (wr < n) {
        ssize_t w = write(fd, (char *)buf + wr, (size_t)(n - wr));
        if (w == -1) die("write");
        wr += w;
    }
    return wr;
}

/* 標本(整数)を複素数へ変換 */
void sample_to_complex(sample_t *s, complex double *X, long n) {
    long i;
    for (i = 0; i < n; i++) X[i] = s[i];
}

/* 複素数を標本(整数)へ変換. 虚数部分は無視 */
void complex_to_sample(complex double *X, sample_t *s, long n) {
    long i;
    for (i = 0; i < n; i++) {
        s[i] = (sample_t)(double)creal(X[i]);
    }
}

/* 高速(逆)フーリエ変換;
   w は1のn乗根.
   フーリエ変換の場合   偏角 -2 pi / n
   逆フーリエ変換の場合 偏角  2 pi / n
   xが入力でyが出力.
   xも破壊される
 */
void fft_r(complex double *x, complex double *y, long n, complex double w) {
    if (n == 1) {
        y[0] = x[0];
    } else {
        complex double W = 1.0;
        long i;
        for (i = 0; i < n / 2; i++) {
            y[i] = (x[i] + x[i + n / 2]);             /* 偶数行 */
            y[i + n / 2] = W * (x[i] - x[i + n / 2]); /* 奇数行 */
            W *= w;
        }
        fft_r(y, x, n / 2, w * w);
        fft_r(y + n / 2, x + n / 2, n / 2, w * w);
        for (i = 0; i < n / 2; i++) {
            y[2 * i] = x[i];
            y[2 * i + 1] = x[i + n / 2];
        }
    }
}

void fft(complex double *x, complex double *y, long n) {
    long i;
    double arg = 2.0 * M_PI / n;
    complex double w = cos(arg) - (complex double)I * sin(arg);
    fft_r(x, y, n, w);
    for (i = 0; i < n; i++) y[i] /= n;
}

void ifft(complex double *y, complex double *x, long n) {
    double arg = 2.0 * M_PI / n;
    complex double w = cos(arg) + (complex double)I * sin(arg);
    fft_r(y, x, n, w);
}

int pow2check(long N) {
    long n = N;
    while (n > 1) {
        if (n % 2) return 0;
        n = n / 2;
    }
    return 1;
}

void print_complex(FILE *wp, complex double *Y, long n) {
    long i;
    for (i = 0; i < n; i++) {
        fprintf(wp, "%ld %f %f %f %f\n", i, creal(Y[i]), cimag(Y[i]),
                cabs(Y[i]), atan2(cimag(Y[i]), creal(Y[i])));
    }
}

void bandpass(complex double *x, long n, double f1, double f2) {
    long l;
    for (l = 1; l < f1 * n / 44100; l++) {
        x[l] = 0.0;
        x[n - l] = 0.0;
    }
    for (l = (long)(f2 * n / 44100 + 1); l < n / 2; l++) {
        x[l] = 0.0;
        x[n - l] = 0.0;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    long n = atol(argv[1]);
    double f1 = atof(argv[2]);
    double f2 = atof(argv[3]);
    if (n < 0 || !pow2check(n)) {
        fprintf(stderr, "error : n (%ld) not a power of two\n", n);
        exit(1);
    }
    if (!(0 <= f1 && f1 <= f2 && f2 < 22050)) {
        fprintf(stderr, "error : must be 0 <= f1 <= f2 < 22050\n");
        exit(1);
    }
    // FILE *wp = fopen("fft.dat", "wb");
    // if (wp == NULL) die("fopen");
    sample_t *buf = calloc(sizeof(sample_t), (size_t)n);
    complex double *X = calloc(sizeof(complex double), (size_t)n);
    complex double *Y = calloc(sizeof(complex double), (size_t)n);
    while (1) {
        /* 標準入力からn個標本を読む */
        ssize_t m = read_n(0, (ssize_t)((size_t)n * sizeof(sample_t)), buf);
        if (m == 0) break;
        /* 複素数の配列に変換 */
        sample_to_complex(buf, X, n);
        /* FFT -> Y */
        fft(X, Y, n);

        // print_complex(wp, Y, n);
        // fprintf(wp, "----------------\n");

        bandpass(Y, n, f1, f2);

        /* IFFT -> Z */
        ifft(Y, X, n);
        /* 標本の配列に変換 */
        complex_to_sample(X, buf, n);
        /* 標準出力へ出力 */
        write_n(1, m, buf);
    }
    // fclose(wp);
    free(buf);
    free(X);
    free(Y);
    return 0;
}
