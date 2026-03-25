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

void print_clients(NetSelectCtx ctx) {
    printf("Clients: ");
    for (int i=0; i < ctx.nodes.len; i++)
        printf("%d ", ctx.nodes.items[i].fd);
    printf("\n");
}

void sigint(int sig) {
    printf("\nTerminating Server.\n");
    exit(0);
}

// Message structure:
// [2 bytes] message length
// [1 byte] typeid describing the format of the message
// [n bytes] body of the message  
//
// [typeid]:
//   [1 byte] message id
//
// [seq]:
//   [2 bytes] sequence number
//
// [string]:
//   [2 bytes] number of bytes in string
//   [n bytes] characters in the string
//
// Client info message:
//   [typeid] 1
//   [seq] sequence number
//   [string] alias
//   NetPackMsg: "%b%w%s"
//
// Ack message:
//   [typeid] 2
//   [seq] client sequence number being responded to
//   [string] additional ack text, usually left blank
//   NetPackMsg: "%b%w%s"
//
// Command message:
//   [typeid] 3
//   [seq] sequence number
//   [string] command
//   Ex command: "list users"
//   NetPackMsg: "%b%w%s"
//
// Chat message:
//   [typeid] 4
//   [seq] sequence number
//   [string] alias from
//   [string] alias to
//   [string] chat text
//   NetPackMsg: "%b%w%s%s%s"
//

// Sequence of messages:
// 1. Client connects to server
// 2. Client sends Client info message to server
// 3. Server sends Ack message back to client
// 4. Client sends message to server
//

void print_msg(char *msg, u16 msglen) {
    u8 typeid = *((u8 *) msg);
    u16 seq=0;

    if (typeid == 1) {
        String alias = StringNew("");
        NetUnpack(msg, msglen, "%b%w%s", &typeid, &seq, &alias);
        printf("** Client Info [%d] alias: '%.*s' **\n", seq, alias.len, alias.bs);

        StringFree(&alias);
    } else if (typeid == 2) {
        String acktext = StringNew("");
        NetUnpack(msg, msglen, "%b%w%s", &typeid, &seq, &acktext);
        printf("** Ack [%d] text: '%.*s' **\n", seq, acktext.len, acktext.bs);

        StringFree(&acktext);
    } else if (typeid == 3) {
        String cmd = StringNew("");
        NetUnpack(msg, msglen, "%b%w%s", &typeid, &seq, &cmd);
        printf("** Command [%d] text: '%.*s' **\n", seq, cmd.len, cmd.bs);

        StringFree(&cmd);
    } else if (typeid == 4) {
        String from_alias = StringNew("");
        String to_alias = StringNew("");
        String text = StringNew("");
        NetUnpack(msg, msglen, "%b%w%s%s%s", &typeid, &seq, &from_alias, &to_alias, &text);
        printf("** Chat [%d] from: '%.*s' to: '%.*s' text: '%.*s' **\n", seq, from_alias.len, from_alias.bs, to_alias.len, to_alias.bs, text.len, text.bs);

        StringFree(&from_alias);
        StringFree(&to_alias);
        StringFree(&text);
    }
}

void client_connected(NetSelectCtx *ctx, NetNode *client) {
    fprintf(stderr, "Connected to client %d\n", client->fd);
}
void client_sent_msg(NetSelectCtx *ctx, NetNode *client, char *msg, u16 msglen) {
    u8 typeid = *((u8 *) msg);
    u16 seq=0;

    if (typeid == 1) {
        String alias = StringNew("");
        NetUnpack(msg, msglen, "%b%w%s", &typeid, &seq, &alias);
        StringAssign(&client->alias, alias.bs);
        StringFree(&alias);

        // Send ack
        typeid = 2;
        char *ackstr = "ack2";
        client->seq++;
        NetPackMsg(&client->writebuf, "%b%w%s", typeid, client->seq, ackstr);
        NetSend2(client->fd, &client->writebuf, ctx);
    } else if (typeid == 4) {
        String from_alias = StringNew("");
        String to_alias = StringNew("");
        String text = StringNew("");
        NetUnpack(msg, msglen, "%b%w%s%s%s", &typeid, &seq, &from_alias, &to_alias, &text);

        // Redirect chat message to to_alias client
        NetNode *to_client = NetNodeArrayFindAlias(ctx->nodes, to_alias.bs);
        if (to_client) {
            client->seq++;
            NetPackMsg(&to_client->writebuf, "%b%w%s%s%s", typeid, client->seq, from_alias.bs, to_alias.bs, text.bs);
            NetSend2(to_client->fd, &to_client->writebuf, ctx);

            // If not all bytes sent, send it in next select() iteration.
            if (to_client->writebuf.len > 0) {
                FD_SET(to_client->fd, &ctx->writefds);
                if (to_client->fd > ctx->maxfd)
                    ctx->maxfd = to_client->fd;
            }
        } else {
            fprintf(stderr, "Can't find to_alias '%.*s' in clients\n", to_alias.len, to_alias.bs);
        }
        StringFree(&from_alias);
        StringFree(&to_alias);
        StringFree(&text);
    }

    print_msg(msg, msglen);
}
void client_end_transmission(NetSelectCtx *ctx, NetNode *client) {
    fprintf(stderr, "Client %d end transmission\n", client->fd);
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
    int s0 = OpenListenSocket(host, port, backlog, &sa);
    if (s0 == -1)
        exit(1);

    String ipaddr = StringNew("");
    GetTextIPAddress(&sa, &ipaddr);
    printf("Listening on %.*s port %s...\n", ipaddr.len, ipaddr.bs, port);
    StringFree(&ipaddr);

    NetSelectCtx ctx;
    NetInit(&ctx, s0);

    fd_set tmp_readfds, tmp_writefds;
    while (1) {
        tmp_readfds = ctx.readfds;
        tmp_writefds = ctx.writefds;
        //fprintf(stderr, "select()...\n");
        z = select(ctx.maxfd+1, &tmp_readfds, &tmp_writefds, NULL, NULL);
        if (z == 0) // timeout
            continue;
        if (z == -1 && errno == EINTR)
            continue;
        if (z == -1) {
            fprintf(stderr, "select(): %s\n", strerror(errno));
            break;
        }

        for (int i=0; i <= ctx.maxfd; i++) {
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
                    FD_SET(clientfd, &ctx.readfds);
                    if (clientfd > ctx.maxfd)
                        ctx.maxfd = clientfd;

                    NetNode client = NetNodeNew(clientfd);
                    NetNodeArrayAppend(&ctx.nodes, client);
                    client_connected(&ctx, &client);
                } else {
                    int clientfd = i;
                    fprintf(stderr, "Received data from client %d\n", clientfd);

                    NetNode *client = NetNodeArrayFind(ctx.nodes, clientfd);
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
                        if (client->msglen == 0) {
                            // Read block length
                            if (readbuf->len >= sizeof(u16)) {
                                u16 *bs = (u16 *) readbuf->bs;
                                client->msglen = ntohs(*bs);
                                if (client->msglen == 0) {
                                    read_eof = 1;
                                    break;
                                }
                                BufferShift(readbuf, sizeof(u16));
                                continue;
                            }
                            break;
                        } else {
                            // Read block body (msglen bytes)
                            if (readbuf->len >= client->msglen) {
                                client_sent_msg(&ctx, client, readbuf->bs, client->msglen);
                                BufferShift(readbuf, client->msglen);
                                client->msglen = 0;

                                // If there are unsent bytes sent from client_received_block()
                                if (writebuf->len > 0) {
                                    FD_SET(clientfd, &ctx.writefds);
                                    if (clientfd > ctx.maxfd)
                                        ctx.maxfd = clientfd;
                                }
                                continue;
                            }
                            break;
                        }
                    }
                    if (read_eof) {
                        client_end_transmission(&ctx, client);
                        FD_CLR(clientfd, &ctx.readfds);
                        shutdown(clientfd, SHUT_RD);
                        client->shut_rd = 1;

                        // Remove client if no remaining reads and writes.
                        if (writebuf->len == 0) {
                            NetNodeArrayRemove(&ctx.nodes, clientfd);
                            FD_CLR(clientfd, &ctx.writefds);
                            shutdown(clientfd, SHUT_WR);
                            close(clientfd);
                        }
                    }
                }
            }
            if (FD_ISSET(i, &tmp_writefds)) {
                int clientfd = i;

                NetNode *client = NetNodeArrayFind(ctx.nodes, clientfd);
                if (client == NULL) {
                    fprintf(stderr, "Can't find client buffer %d\n", clientfd);
                    continue;
                }

                Buffer *writebuf = &client->writebuf;
                NetSend2(clientfd, writebuf, &ctx);

                // Remove client if no remaining reads and writes.
                if (writebuf->len == 0 && client->shut_rd) {
                    NetNodeArrayRemove(&ctx.nodes, clientfd);
                    shutdown(clientfd, SHUT_WR);
                    close(clientfd);
                }
            }
        }
    }

    close(s0);

    return 0;
}

