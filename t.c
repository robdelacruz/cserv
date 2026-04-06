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
#include "msg.h"
#include "data.h"

ServerData serverdata;

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

void client_connected(NetSelectCtx *ctx, NetNode *client) {
    fprintf(stderr, "Connected to client %d\n", client->fd);
}
void client_sent_msg(NetSelectCtx *ctx, NetNode *client, char *msgbytes, u16 len) {
    int z;
    Msg msg;
    MsgUnpack(&msg, msgbytes, len);
    u8 msgno = MSGNO(&msg);
    if (msgno == 0) {
        return;
    }
    if (msgno == COMMANDMSG) {
        CommandMsg *p = (CommandMsg *) &msg;
        if (StringEquals(p->command, "list users")) {
            // Return aliases response
            AliasesMsg resp_msg = {ALIASESMSG, StringNew("admin;robtwister;user1")};
            MsgPack(&resp_msg, &client->writebuf);
            MsgFree(&resp_msg);
            NetSend2(client->fd, &client->writebuf, ctx);
        }
    } else if (msgno == REGISTERMSG) {
        RegisterMsg *p = (RegisterMsg *) &msg;
        int z = RegisterUser(&serverdata, p->alias.bs, p->pwd.bs);
        // Return status response
        StatusMsg resp_msg = {STATUSMSG, z, StringNew(server_strerror(z))};
        MsgPack(&resp_msg, &client->writebuf);
        MsgFree(&resp_msg);
        NetSend2(client->fd, &client->writebuf, ctx);
        if (z != 0)
            ServerDataSave(serverdata);
    }

    MsgPrint(&msg);
    MsgFree(&msg);
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

    serverdata = ServerDataNew();
    ServerDataLoad(&serverdata);

    int backlog = 50;
    struct sockaddr sa;
    int s0 = OpenListenSocket(host, port, backlog, &sa);
    if (s0 == -1)
        exit(1);

    String ipaddr = StringNew("");
    GetTextIPAddress(&sa, &ipaddr);
    printf("Listening on %.*s port %s...\n", ipaddr.len, ipaddr.bs, port);
    StringFree(ipaddr);

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

                    NetNode *client = NetNodeArrayFind(ctx.nodes, clientfd);
                    if (client == NULL) {
                        fprintf(stderr, "Can't find client buffer %d\n", clientfd);
                        continue;
                    }

                    int read_eof = 0;
                    if (NetRecv(clientfd, &client->readbuf) == 0)
                        read_eof = 1;

                    // Each message is a sequence of bytes.
                    // To send a message, first send a 16bit value containing the size of the message (msglen)
                    // followed by the stream of msglen message bytes.
                    // To terminate the stream, send a 0 msglen value.

                    Buffer *readbuf = &client->readbuf;
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
                        if (client->writebuf.len == 0) {
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

                NetSend2(clientfd, &client->writebuf, &ctx);

                // Remove client if no remaining reads and writes.
                if (client->writebuf.len == 0 && client->shut_rd) {
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

