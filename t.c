#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clib.h"

int open_listen_socket(char *host, char *port, int backlog, struct sockaddr *sa) {
    int z;
    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo(host, port, &hints, &ai);
    if (z != 0) {
        fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(z));
        errno = EINVAL;
        return -1;
    }
    if (sa != NULL)
        memcpy(sa, ai->ai_addr, ai->ai_addrlen);

    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1) {
        fprintf(stderr, "socket(): %s\n", strerror(errno));
        freeaddrinfo(ai);
        return -1;
    }
    int yes=1;
    z = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (z == -1) {
        fprintf(stderr, "setsockopt(): %s\n", strerror(errno));
        freeaddrinfo(ai);
        return -1;
    }
    z = bind(fd, ai->ai_addr, ai->ai_addrlen);
    if (z == -1) {
        fprintf(stderr, "bind(): %s\n", strerror(errno));
        freeaddrinfo(ai);
        return -1;
    }
    z = listen(fd, backlog);
    if (z == -1) {
        fprintf(stderr, "listen(): %s\n", strerror(errno));
        freeaddrinfo(ai);
        return -1;
    }
    freeaddrinfo(ai);
    return fd;
}

String make_ipaddr_string(struct sockaddr *sa) {
    void *sin_addr;
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in *) sa;
        sin_addr = &p->sin_addr;
    } else {
        struct sockaddr_in6 *p = (struct sockaddr_in6 *) sa;
        sin_addr = &p->sin6_addr;
    }
    char bs[INET6_ADDRSTRLEN];
    if (inet_ntop(sa->sa_family, sin_addr, bs, sizeof(bs)) == NULL) {
        fprintf(stderr, "inet_ntop(): %s\n", strerror(errno));
        return StringNew("");
    }
    return StringNew(bs);
}

int main(int argc, char *argv[]) {
    char *host = "localhost";
    char *port = "8000";
    if (argc > 1)
        host = argv[1];
    if (argc > 2)
        port = argv[2];

    int backlog = 50;
    struct sockaddr sa;
    int s0 = open_listen_socket(host, port, backlog, &sa);
    if (s0 == -1)
        exit(1);

    String ipaddr = make_ipaddr_string(&sa);
    printf("Listening on %.*s port %s...\n", ipaddr.len, ipaddr.bs, port);
    StringFree(&ipaddr);

    close(s0);

    return 0;
}

