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
    struct _Client *next;
} Client;

Client *client_head = NULL;

Client *add_new_client(int clientfd, Client **head) {
    Client *client = (Client *) malloc(sizeof(Client));
    client->fd = clientfd;
    client->buf = BufferNew(4096);
    client->next = NULL;
    if (*head == NULL) {
        *head = client;
    } else {
        Client *node = *head;
        while (node->next != NULL)
            node = node->next;
        node->next = client;
    }
    return client;
}

void remove_client(int clientfd, Client **head) {
    Client *node = *head;
    Client *prev = NULL;
    while (node != NULL) {
        if (node->fd == clientfd) {
            if (prev == NULL)
                *head = node->next;
            else
                prev->next = node->next;
            BufferFree(&node->buf);
            free(node);
            break;
        }
        prev = node;
        node = node->next;
    }
}

Client *find_client(int clientfd, Client *head) {
    Client *client = head;
    while (client != NULL) {
        if (client->fd == clientfd)
            break;
        client = client->next;
    }
    return client;
}

void print_clients() {
    printf("Clients: ");
    Client *client = client_head;
    while (client != NULL) {
        printf("%d ", client->fd);
        client = client->next;
    }
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

                    add_new_client(clientfd, &client_head);
                } else {
                    int clientfd = i;
                    fprintf(stderr, "Received data from client %d\n", clientfd);

                    Client *client = find_client(clientfd, client_head);
                    if (client == NULL) {
                        fprintf(stderr, "Can't find client buffer %d\n", clientfd);
                        continue;
                    }

                    z = read_sock(clientfd, &client->buf);
                    if (z == 0) {
                        // Client socket closed, close socket and remove client from list.
                        fprintf(stderr, "Client %d closed\n", clientfd);

                        remove_client(clientfd, &client_head);
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

