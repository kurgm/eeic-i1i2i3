#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define N 1000

static char data[N];
int write_force(int fildes, const void *ptr, size_t nbyte);

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

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s IPADDRESS PORT\n", argv[0]);
        return 1;
    }
    int s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s == -1) {
        perror("socket");
        return 2;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    if (inet_aton(argv[1], &addr.sin_addr) == 0) {
        fprintf(stderr, "invalid ipaddress\n");
        return 2;
    }
    addr.sin_port = htons(atoi(argv[2]));
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        return 2;
    }
    data[0] = 42;
    if (send(s, data, 1, 0) == -1) {
        perror("send failed");
        return 1;
    }
    while (1) {
        ssize_t n = recvfrom(s, data, sizeof(data), 0, NULL, NULL);
        if (n == -1) {
            perror("recvfrom failed");
            return 1;
        }
        if (n == 1000) {
            int end = 1;
            for (size_t i = 0; i < sizeof(data) / sizeof(char); i++) {
                if (data[i] != 1) {
                    end = 0;
                    break;
                }
            }
            if (end) {
                break;
            }
        }
        if (write_force(1, data, sizeof(char) * (size_t)n)) {
            return 1;
        }
    }
    return 0;
}
