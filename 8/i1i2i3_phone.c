#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define N 256

static char data[N];
int write_force(int fildes, const void *ptr, size_t nbyte);
int send_force(int socket, const void *buffer, size_t length, int flags);
void *stdinreader(void *arg);

int write_force(int fildes, const void *ptr, size_t nbyte) {
    size_t written = 0;
    while (written < nbyte) {
        ssize_t written2 =
            write(fildes, (const char *)ptr + written, nbyte - written);
        if (written2 == -1) {
            perror("write failed");
            return 1;
        }
        written += (size_t)written2;
    }
    return 0;
}

int send_force(int socket, const void *buffer, size_t length, int flags) {
    size_t sent = 0;
    while (sent < length) {
        ssize_t sent2 =
            send(socket, (const char *)buffer + sent, length - sent, flags);
        if (sent2 == -1) {
            perror("send failed");
            return 1;
        }
        sent += (size_t)sent2;
    }
    return 0;
}

static int state = 0;
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;

void *stdinreader(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&state_lock);
        int current_state = state;
        pthread_mutex_unlock(&state_lock);
        if (current_state == 1) {
            break;
        }
        ssize_t n = read(0, data, N);
        if (n == -1) {
            perror("read failed");
            return (void *)1;
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s ADDRESS PORT\n", argv[0]);
        return 1;
    }
    pthread_t threadid;
    int err = pthread_create(&threadid, NULL, stdinreader, NULL);
    if (err != 0) {
        errno = err;
        perror("pthread_create");
        return 2;
    }
    int ss = socket(PF_INET, SOCK_STREAM, 0);
    if (ss == -1) {
        perror("socket");
        return 2;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    bool is_server = strcmp(argv[1], "-l") == 0;
    int s;
    if (is_server) {
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(ss, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            perror("bind");
            close(ss);
            return 2;
        }
        if (listen(ss, 10) == -1) {
            perror("listen");
            close(ss);
            return 2;
        }
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(struct sockaddr_in);
        s = accept(ss, (struct sockaddr *)&client_addr, &len);
        if (s == -1) {
            perror("accept");
            close(ss);
            return 2;
        }
        fprintf(stderr, "connected from %s:%u\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    } else {
        if (inet_aton(argv[1], &addr.sin_addr) == 0) {
            fprintf(stderr, "invalid ipaddress\n");
            return 2;
        }
        s = ss;
        if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            perror("connect");
            return 2;
        }
        fprintf(stderr, "connected\n");
    }
    pthread_mutex_lock(&state_lock);
    state = 1;
    pthread_mutex_unlock(&state_lock);
    void *ret;
    pthread_join(threadid, &ret);
    if (ret != NULL) {
        close(s);
        if (is_server) {
            close(ss);
        }
        return 2;
    }
    int failed = 0;
    while (!failed) {
        ssize_t n = read(0, data, sizeof(data));
        if (n == 0) {
            break;
        }
        if (n == -1) {
            perror("read failed");
            failed = 1;
            break;
        }
        failed = send_force(s, data, sizeof(char) * (size_t)n, 0);
        if (failed) {
            break;
        }
        n = recv(s, data, sizeof(data), 0);
        if (n == 0) {
            break;
        }
        if (n == -1) {
            perror("recv failed");
            failed = 1;
            break;
        }
        failed = write_force(1, data, sizeof(char) * (size_t)n);
    }
    close(s);
    if (is_server) {
        close(ss);
    }
    return failed;
}
