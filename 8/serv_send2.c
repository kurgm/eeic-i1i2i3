#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define N 256

static char data[N];
int send_force(int socket, const void *buffer, size_t length, int flags);
void *stdinreader(void *arg);

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
    if (argc != 2) {
        fprintf(stderr, "usage: %s PORT\n", argv[0]);
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
    addr.sin_port = htons(atoi(argv[1]));
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
    int s = accept(ss, (struct sockaddr *)&client_addr, &len);
    if (s == -1) {
        perror("accept");
        close(ss);
        return 2;
    }
    pthread_mutex_lock(&state_lock);
    state = 1;
    pthread_mutex_unlock(&state_lock);
    void *ret;
    pthread_join(threadid, &ret);
    if (ret != NULL) {
        close(s);
        close(ss);
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
    }
    close(s);
    close(ss);
    return failed;
}
