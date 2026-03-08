#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clib.h"
#include "cnet.h"

ClientArray _clients;

void print_clients() {
    printf("Clients: ");
    for (int i=0; i < _clients.len; i++)
        printf("%d ", _clients.items[i].fd);
    printf("\n");
}

void sigint(int sig) {
    printf("\nTerminating Server.\n");
    exit(0);
}

void client_connected(int clientfd) {
    fprintf(stderr, "Connected to client %d\n", clientfd);
}
void client_endtransmission(int clientfd) {
    printf("Client %d end transmission\n", clientfd);
}
void client_received_block(int clientfd, char *blk, u16 blk_len) {
    printf("Client %d received block '%.*s'\n", clientfd, blk_len, blk);
}
void client_close(int clientfd) {
    fprintf(stderr, "Close client %d\n", clientfd);
}

int main(int argc, char *argv[]) {
    int z;
    char *host = "localhost";
    char *port = "8000";
    if (argc > 1)
        host = argv[1];
    if (argc > 2)
        port = argv[2];

    signal(SIGINT, sigint);

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
        //fprintf(stderr, "select()...\n");
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
                    FD_SET(clientfd, &readfds);
                    if (clientfd > maxfd)
                        maxfd = clientfd;

                    Client client = ClientNew(clientfd);
                    ClientArrayAppend(&_clients, client);
                    client_connected(clientfd);
                } else {
                    int clientfd = i;
                    fprintf(stderr, "Received data from client %d\n", clientfd);

                    Client *client = ClientArrayFind(&_clients, clientfd);
                    if (client == NULL) {
                        fprintf(stderr, "Can't find client buffer %d\n", clientfd);
                        continue;
                    }

                    int close_client = 0;
                    if (read_sock(clientfd, &client->buf) == 0)
                        close_client = 1;

                    // Message format:
                    // [block 1], [block 2], ... [0]
                    //
                    // [u16] block length
                    // [block length bytes] block body
                    // [u16] next block length
                    // [next block length bytes] next block body
                    // [u16] 0 (zero block length, end of blocks)

                    Buffer *buf = &client->buf;
                    while (1) {
                        //printf("buf->len: %ld buf->cur: %ld\n", buf->len, buf->cur);
                        if (client->blk_len == 0) {
                            // Read block length
                            if (buf->len-buf->cur >= sizeof(u16)) {
                                u16 *bs = (u16 *) &buf->bs[buf->cur];
                                u16 blk_len = ntohs(*bs);
                                if (blk_len == 0) {
                                    client_endtransmission(clientfd);
                                    close_client = 1;
                                    break;
                                }

                                client->blk_len = blk_len;
                                buf->cur += sizeof(u16);
                                continue;
                            } else {
                                break;
                            }
                        } else {
                            // Read block body (blk_len bytes)
                            if (buf->len-buf->cur >= client->blk_len) {
                                client_received_block(clientfd, buf->bs+buf->cur, client->blk_len);
                                buf->cur += client->blk_len;
                                client->blk_len = 0;
                                continue;
                            } else {
                                break;
                            }
                        }
                    }

                    if (close_client) {
                        client_close(clientfd);
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

