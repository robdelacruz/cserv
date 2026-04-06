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

void sigint(int sig) {
    printf("\nTerminating Server.\n");
    exit(0);
}

void on_host_connected(SelectCtx *ctx, HostCtx *hostctx) {
    fprintf(stderr, "Connected to client %d\n", hostctx->fd);
}
void on_host_recv_msg(SelectCtx *ctx, HostCtx *hostctx, char *msgbytes, u16 len) {
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
            MsgPack(&resp_msg, &hostctx->writebuf);
            MsgFree(&resp_msg);
            NetSend2(hostctx->fd, &hostctx->writebuf, ctx);
        }
    } else if (msgno == REGISTERMSG) {
        RegisterMsg *p = (RegisterMsg *) &msg;
        int z = RegisterUser(&serverdata, p->alias.bs, p->pwd.bs);
        // Return status response
        StatusMsg resp_msg = {STATUSMSG, z, StringNew(server_strerror(z))};
        MsgPack(&resp_msg, &hostctx->writebuf);
        MsgFree(&resp_msg);
        NetSend2(hostctx->fd, &hostctx->writebuf, ctx);
        if (z != 0)
            ServerDataSave(serverdata);
    }

    MsgPrint(&msg);
    MsgFree(&msg);
}
void on_host_eof(SelectCtx *ctx, HostCtx *hostctx) {
    fprintf(stderr, "Client %d end transmission\n", hostctx->fd);
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

    SelectCtx selectctx;
    NetInit(&selectctx, s0);

    fd_set tmp_readfds, tmp_writefds;
    while (1) {
        tmp_readfds = selectctx.readfds;
        tmp_writefds = selectctx.writefds;
        //fprintf(stderr, "select()...\n");
        z = select(selectctx.maxfd+1, &tmp_readfds, &tmp_writefds, NULL, NULL);
        if (z == 0) // timeout
            continue;
        if (z == -1 && errno == EINTR)
            continue;
        if (z == -1) {
            fprintf(stderr, "select(): %s\n", strerror(errno));
            break;
        }

        for (int i=0; i <= selectctx.maxfd; i++) {
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
                    FD_SET(clientfd, &selectctx.readfds);
                    if (clientfd > selectctx.maxfd)
                        selectctx.maxfd = clientfd;

                    HostCtx hostctx = HostCtxNew(clientfd);
                    HostCtxArrayAppend(&selectctx.hostctxs, hostctx);
                    on_host_connected(&selectctx, &hostctx);
                } else {
                    int clientfd = i;

                    HostCtx *hostctx = HostCtxArrayFind(selectctx.hostctxs, clientfd);
                    if (hostctx == NULL) {
                        fprintf(stderr, "Can't find client buffer %d\n", clientfd);
                        continue;
                    }

                    int read_eof = 0;
                    if (NetRecv(clientfd, &hostctx->readbuf) == 0)
                        read_eof = 1;

                    // Each message is a 16bit msglen value followed by msglen sequence of bytes.
                    // A msglen of 0 means no more bytes remaining in the stream.

                    Buffer *readbuf = &hostctx->readbuf;
                    while (1) {
                        if (hostctx->msglen == 0) {
                            // Read block length
                            if (readbuf->len >= sizeof(u16)) {
                                u16 *bs = (u16 *) readbuf->bs;
                                hostctx->msglen = ntohs(*bs);
                                if (hostctx->msglen == 0) {
                                    read_eof = 1;
                                    break;
                                }
                                BufferShift(readbuf, sizeof(u16));
                                continue;
                            }
                            break;
                        } else {
                            // Read block body (msglen bytes)
                            if (readbuf->len >= hostctx->msglen) {
                                on_host_recv_msg(&selectctx, hostctx, readbuf->bs, hostctx->msglen);
                                BufferShift(readbuf, hostctx->msglen);
                                hostctx->msglen = 0;
                                continue;
                            }
                            break;
                        }
                    }
                    if (read_eof) {
                        on_host_eof(&selectctx, hostctx);
                        FD_CLR(clientfd, &selectctx.readfds);
                        shutdown(clientfd, SHUT_RD);
                        hostctx->shut_rd = 1;

                        // Remove hostctx if no remaining reads and writes.
                        if (hostctx->writebuf.len == 0) {
                            HostCtxArrayRemove(&selectctx.hostctxs, clientfd);
                            FD_CLR(clientfd, &selectctx.writefds);
                            shutdown(clientfd, SHUT_WR);
                            close(clientfd);
                        }
                    }
                }
            }
            if (FD_ISSET(i, &tmp_writefds)) {
                int clientfd = i;

                HostCtx *hostctx = HostCtxArrayFind(selectctx.hostctxs, clientfd);
                if (hostctx == NULL) {
                    fprintf(stderr, "Can't find client buffer %d\n", clientfd);
                    continue;
                }

                NetSend2(clientfd, &hostctx->writebuf, &selectctx);

                // Remove hostctx if no remaining reads and writes.
                if (hostctx->writebuf.len == 0 && hostctx->shut_rd) {
                    HostCtxArrayRemove(&selectctx.hostctxs, clientfd);
                    shutdown(clientfd, SHUT_WR);
                    close(clientfd);
                }
            }
        }
    }

    close(s0);

    return 0;
}

