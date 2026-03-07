#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

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

typedef struct _Client {
    int fd;
    Buffer buf;
} Client;

typedef struct {
    Client *items;
    u16 len;
    u16 cap;
    i8 isfreeitems;
} ClientArray;

Client ClientNew(int fd) {
    Client client;
    client.fd = fd;
    client.buf = BufferNew(4096);
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

ClientArray _clients;

void print_clients() {
    printf("Clients: ");
    for (int i=0; i < _clients.len; i++)
        printf("%d ", _clients.items[i].fd);
    printf("\n");
}

int main(int argc, char *argv[]) {
    int z;
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

    fd_set readfds, writefds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    int maxfd=0;

    FD_SET(s0, &readfds);
    maxfd = s0;

    _clients = ClientArrayNew(255);

    fd_set tmp_readfds, tmp_writefds;
    while (1) {
        tmp_readfds = readfds;
        tmp_writefds = writefds;
        fprintf(stderr, "select()...\n");
        z = select(maxfd+1, &tmp_readfds, &tmp_writefds, NULL, NULL);
        if (z == 0) // timeout
            continue;
        if (z == -1 && errno == EINTR)
            continue;
        if (z == -1) {
            fprintf(stderr, "select(): %s\n", strerror(errno));
            break;
        }

        for (int i=0; i <= maxfd; i++) {
            if (FD_ISSET(i, &tmp_readfds)) {
                // New client connection, ready to receive data from client.
                if (i == s0) {
                    socklen_t sa_len = sizeof(struct sockaddr_in);
                    struct sockaddr_in sa;
                    int clientfd = accept(s0, (struct sockaddr *) &sa, &sa_len);
                    if (clientfd == -1) {
                        fprintf(stderr, "accept(): %s\n", strerror(errno));
                        continue;
                    }
                    fprintf(stderr, "Connected to client %d\n", clientfd);
                    FD_SET(clientfd, &readfds);
                    if (clientfd > maxfd)
                        maxfd = clientfd;

                    Client client = ClientNew(clientfd);
                    ClientArrayAppend(&_clients, client);
                } else {
                    int clientfd = i;
                    fprintf(stderr, "Received data from client %d\n", clientfd);

                    Client *client = NULL;
                    for (int i=0; i < _clients.len; i++) {
                        if (_clients.items[i].fd == clientfd)
                            client = &_clients.items[i];
                    }
                    if (client == NULL) {
                        fprintf(stderr, "Can't find client buffer %d\n", clientfd);
                        continue;
                    }

                    z = read_sock(clientfd, &client->buf);
                    if (z == 0) {
                        // Client socket closed, close socket and remove client from list.
                        fprintf(stderr, "Client %d closed\n", clientfd);

                        ClientArrayRemove(&_clients, clientfd);
                        FD_CLR(clientfd, &readfds);
                        close(clientfd);
                    }
                }
            }
        }
    }

    close(s0);

    return 0;
}

