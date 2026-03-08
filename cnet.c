#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "clib.h"
#include "cnet.h"

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

// Nonblocking socket read into buf.
// Returns  0 for EOF (socket was shutdown)
//         -1 if error occured (check errno)
//          1 if socket is still open for receiving data
int read_sock(int fd, Buffer *buf) {
    int z;
    char readbuf[1024];
    while (1) {
        z = recv(fd, readbuf, sizeof(readbuf), MSG_DONTWAIT);
        if (z == 0)
            return 0;
        if (z == -1 && errno == EINTR)
            continue;
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return 1;
        if (z == -1) {
            fprintf(stderr, "recv() on socket %d: %s\n", fd, strerror(errno));
            return -1; 
        }
        assert(z > 0);
        BufferAppend(buf, readbuf, z);
    }
    return 1;
}

Client ClientNew(int fd) {
    Client client;
    client.fd = fd;
    client.buf = BufferNew(4096);
    client.blk_len = 0;
    return client;
}
void ClientFree(Client *client) {
    client->fd = 0;
    BufferFree(&client->buf);
}

ClientArray ClientArrayNew(u16 cap) {
    ClientArray ca;
    if (cap == 0)
        cap = 32;
    ca.items = (Client *) malloc(sizeof(Client)*cap);
    memset(ca.items, 0, sizeof(Client)*cap);
    ca.len = 0;
    ca.cap = cap;
    ca.isfreeitems = 0;
    return ca;
}
void ClientArrayFree(ClientArray *ca) {
    if (ca->isfreeitems) {
        for (int i=0; i < ca->len; i++)
            ClientFree(&ca->items[i]);
    }
    free(ca->items);
    ca->items = 0;
    ca->len = 0;
}
void ClientArrayClear(ClientArray *ca) {
    memset(ca->items, 0, sizeof(sizeof(Client)*ca->len));
    ca->len = 0;
}
void ClientArrayAppend(ClientArray *ca, Client client) {
    assert(ca->len <= ca->cap);

    // Double the capacity if more space needed.
    if (ca->len == ca->cap) {
        ca->items = (Client *) realloc(ca->items, sizeof(Client)*ca->cap * 2);
        memset(ca->items + sizeof(Client)*ca->cap, 0, sizeof(Client)*ca->cap);
        ca->cap *= 2;
    }
    assert(ca->len < ca->cap);

    ca->items[ca->len] = client;
    ca->len++;
}
void ClientArrayRemove(ClientArray *ca, int fd) {
    int i;
    for (i=0; i < ca->len; i++) {
        if (ca->items[i].fd == fd)
            break;
    }
    if (i == ca->len)
        return;
    // Move last item to the spot where the deleted item is.
    ca->items[i] = ca->items[ca->len-1];

    memset(&ca->items[ca->len-1], 0, sizeof(Client));
    ca->len--;
}
Client *ClientArrayFind(ClientArray *ca, int fd) {
    for (int i=0; i < ca->len; i++) {
        if (ca->items[i].fd == fd)
            return &ca->items[i];
    }
    return NULL;
}

