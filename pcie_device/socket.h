#pragma once
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

struct socket {
    struct sockaddr_in addr;
    int fd;
};

static inline void socket_init(struct socket *s) {
    s->fd = socket(AF_INET, SOCK_STREAM, 0);
    s->addr.sin_family = AF_INET;
}

static inline int socket_connect(struct socket *s, const char *host, int port) {
    s->addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &s->addr.sin_addr);
    return connect(s->fd, (struct sockaddr *)&s->addr, sizeof(s->addr)) == 0;
}

static inline int socket_send(const struct socket *s, const char *msg) {
    return send(s->fd, msg, strlen(msg), 0) != -1;
}

static inline void socket_close(struct socket *s) {
    close(s->fd);
    memset(s, 0, sizeof(*s));
}
