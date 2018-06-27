#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "forceio.h"
#include "videotalk.h"

#define N 2048

static char data[N];
void *stdinreader(void *arg);

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
    if (argc != 6) {
        fprintf(stderr,
                "usage: %s ADDRESS PORT1 PORT2 VIDEO_OUTPUT1 VIDEO_OUTPUT2\n",
                argv[0]);
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
    int v_s = socket(PF_INET, SOCK_DGRAM, 0);
    if (v_s == -1) {
        perror("socket");
        close(ss);
        return 2;
    }
    struct sockaddr_in addr1, addr2;
    addr1.sin_family = AF_INET;
    addr1.sin_port = htons((uint16_t)atoi(argv[2]));
    addr2.sin_family = AF_INET;
    addr2.sin_port = htons((uint16_t)atoi(argv[3]));
    struct sockaddr_in client_addr_video;
    bool is_server = strcmp(argv[1], "-l") == 0;
    int s;
    if (is_server) {
        int soval = 1;
        if (setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, &soval, sizeof(soval)) ==
                -1 ||
            setsockopt(v_s, SOL_SOCKET, SO_REUSEADDR, &soval, sizeof(soval)) ==
                -1) {
            perror("setsockopt");
            close(ss);
            close(v_s);
            return 2;
        }
        addr1.sin_addr.s_addr = addr2.sin_addr.s_addr = INADDR_ANY;
        if (bind(ss, (struct sockaddr *)&addr1, sizeof(addr1)) == -1 ||
            bind(v_s, (struct sockaddr *)&addr2, sizeof(addr2)) == -1) {
            perror("bind");
            close(ss);
            close(v_s);
            return 2;
        }
        if (listen(ss, 10) == -1) {
            perror("listen");
            close(ss);
            close(v_s);
            return 2;
        }
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        s = accept(ss, (struct sockaddr *)&client_addr, &len);
        close(ss);
        if (s == -1) {
            perror("accept");
            close(v_s);
            return 2;
        }
        fprintf(stderr, "connected from %s:%u\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        socklen_t len_v = sizeof(client_addr_video);
        char buf[3];
        ssize_t n;
        do {
            n = recvfrom(v_s, buf, sizeof(buf), 0,
                         (struct sockaddr *)&client_addr_video, &len_v);
            assert(len_v == sizeof(client_addr_video));
            if (n == -1) {
                perror("recvfrom");
                close(s);
                close(v_s);
                return 2;
            }
        } while (!(n >= 3 && buf[0] == 'C' && buf[1] == 'O' && buf[2] == 'N'));
        if (connect(v_s, (struct sockaddr *)&client_addr_video, len_v) == -1) {
            perror("connect");
            close(s);
            close(v_s);
            return 2;
        }
        fprintf(stderr, "connected video from %s:%u\n",
                inet_ntoa(client_addr_video.sin_addr),
                ntohs(client_addr_video.sin_port));
    } else {
        if (inet_aton(argv[1], &addr1.sin_addr) == 0 ||
            inet_aton(argv[1], &addr2.sin_addr) == 0) {
            fprintf(stderr, "invalid ipaddress\n");
            close(ss);
            close(v_s);
            return 2;
        }
        s = ss;
        if (connect(s, (struct sockaddr *)&addr1, sizeof(addr1)) == -1 ||
            connect(v_s, (struct sockaddr *)&addr2, sizeof(addr2)) == -1) {
            perror("connect");
            close(ss);
            close(v_s);
            return 2;
        }
        fprintf(stderr, "connected\n");
        char buf[3] = {'C', 'O', 'N'};
        if (send(v_s, buf, 3, 0) == -1) {
            close(ss);
            close(v_s);
            return 2;
        }
    }
    pthread_mutex_lock(&state_lock);
    state = 1;
    pthread_mutex_unlock(&state_lock);
    void *ret;
    pthread_join(threadid, &ret);
    if (ret != NULL) {
        close(s);
        close(v_s);
        return 2;
    }
    int vo1fd = open(argv[4], O_WRONLY);
    int vo2fd = open(argv[5], O_WRONLY);
    if (vo1fd == -1 || vo2fd == -1) {
        perror("open");
        if (vo1fd != -1) {
            close(vo1fd);
        }
        if (vo2fd != -1) {
            close(vo2fd);
        }
        close(s);
        close(v_s);
        return 2;
    }

    pthread_t v_threadid;
    video_args varg = {
        .videoout1fd = vo1fd, .videoout2fd = vo2fd, .sockfd = v_s};
    err = pthread_create(&v_threadid, NULL, videotalk, &varg);
    if (err != 0) {
        errno = err;
        perror("pthread_create");
        close(s);
        close(v_s);
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
    close(v_s);
    return failed;
}
