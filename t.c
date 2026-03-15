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

void client_connected(Client *client) {
    fprintf(stderr, "Connected to client %d\n", client->fd);
}
void client_received_block(Client *client, char *blk, u16 blk_len) {
    printf("Client %d received block '%.*s'\n", client->fd, blk_len, blk);
}
void client_end_transmission(Client *client) {
    fprintf(stderr, "Client %d end transmission\n", client->fd);
}

// Message components:
// typeid: 1-byte identifier representing what type of message it is
// string: u16 length followed by length bytes representing a string
// Each message starts with a typeid followed by the message body.
//
// Message types:
//
// Client intro
// typeid: 1
// string: alias
//
// Command
// typeid: 2
// string: command
// Available command strings: "list users"
//
// Send chat
// typeid: 3
// string: alias from
// string: alias to
// string: chat text
//
//
//

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
    int s0 = OpenListenSocket(host, port, backlog, &sa);
    if (s0 == -1)
        exit(1);

    String ipaddr = StringNew("");
    GetTextIPAddress(&sa, &ipaddr);
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
                    client_connected(&client);
                } else {
                    int clientfd = i;
                    fprintf(stderr, "Received data from client %d\n", clientfd);

                    Client *client = ClientArrayFind(&_clients, clientfd);
                    if (client == NULL) {
                        fprintf(stderr, "Can't find client buffer %d\n", clientfd);
                        continue;
                    }

                    int read_eof = 0;
                    if (NetRecv(clientfd, &client->readbuf) == 0)
                        read_eof = 1;

                    // Message format:
                    // [block 1], [block 2], ... [0]
                    //
                    // [u16] block length
                    // [block length bytes] block body
                    // [u16] next block length
                    // [next block length bytes] next block body
                    // [u16] 0 (zero block length, end of blocks)

                    Buffer *readbuf = &client->readbuf;
                    Buffer *writebuf = &client->writebuf;
                    while (1) {
                        if (client->blk_len == 0) {
                            // Read block length
                            if (readbuf->len >= sizeof(u16)) {
                                u16 *bs = (u16 *) readbuf->bs;
                                client->blk_len = ntohs(*bs);
                                if (client->blk_len == 0) {
                                    read_eof = 1;
                                    break;
                                }
                                BufferShift(readbuf, sizeof(u16));
                                continue;
                            }
                            break;
                        } else {
                            // Read block body (blk_len bytes)
                            if (readbuf->len >= client->blk_len) {
                                u16 writebuf_org_len = writebuf->len;
                                client_received_block(client, readbuf->bs, client->blk_len);
                                BufferShift(readbuf, client->blk_len);
                                client->blk_len = 0;

                                // client->writebuf contains response, if any
                                if (writebuf->len > writebuf_org_len)
                                    FD_SET(clientfd, &writefds);
                                continue;
                            }
                            break;
                        }
                    }
                    if (read_eof) {
                        client_end_transmission(client);
                        FD_CLR(clientfd, &readfds);
                        shutdown(clientfd, SHUT_RD);
                        client->shut_rd = 1;

                        // Remove client if no remaining reads and writes.
                        if (writebuf->len == 0) {
                            ClientArrayRemove(&_clients, clientfd);
                            FD_CLR(clientfd, &writefds);
                            shutdown(clientfd, SHUT_WR);
                            close(clientfd);
                        }
                    }
                }
            }
            if (FD_ISSET(i, &tmp_writefds)) {
                int clientfd = i;
//                fprintf(stderr, "Sending data to client %d\n", clientfd);

                Client *client = ClientArrayFind(&_clients, clientfd);
                if (client == NULL) {
                    fprintf(stderr, "Can't find client buffer %d\n", clientfd);
                    continue;
                }

                Buffer *writebuf = &client->writebuf;
                NetSend(clientfd, writebuf);

                // Remove client if no remaining reads and writes.
                if (writebuf->len == 0 && client->shut_rd) {
                    ClientArrayRemove(&_clients, clientfd);
                    FD_CLR(clientfd, &writefds);
                    shutdown(clientfd, SHUT_WR);
                    close(clientfd);
                }
            }
        }
    }

    close(s0);

    return 0;
}

