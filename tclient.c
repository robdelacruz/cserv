#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clib.h"
#include "cnet.h"
#include "msg.h"

void sigint(int sig) {
    printf("\nTerminating Server.\n");
    exit(0);
}

// Client states
#define CONNECTED 1
#define WAITING_CLIENT_INFO_ACK 2
#define READY 3
#define WAITING_RESPONSE 4

int clientstate = 0;

void server_sent_msg(NetSelectCtx *ctx, NetNode *server, char *msgbytes, u16 len) {
    u8 msgid = MSGID_FROM_MSGBYTES(msgbytes);
    u16 seq = 0;

    if (clientstate == WAITING_CLIENT_INFO_ACK) {
        if (msgid == MSGID_ACK) {
            AckMsg *p = unpack_message(msgbytes, len);
            printf("Server sent ack '%.*s'\n", p->acktext.len, p->acktext.bs);
            free_message(p);

            clientstate = READY;
        }
    }
}
void server_end_transmission(NetSelectCtx *ctx, NetNode *server) {
    fprintf(stderr, "Server %d end transmission\n", server->fd);
}

int main(int argc, char *argv[]) {
    int z;
    char *serverhost = "localhost";
    char *serverport = "8000";
    if (argc > 1)
        serverhost = argv[1];
    if (argc > 2)
        serverport = argv[2];

    signal(SIGINT, sigint);

    int backlog = 50;
    struct sockaddr sa;
    int serverfd = OpenConnectSocket(serverhost, serverport, backlog, &sa);
    if (serverfd == -1)
        exit(1);

    String ipaddr = StringNew("");
    GetTextIPAddress(&sa, &ipaddr);
    printf("Connected to %.*s port %s...\n", ipaddr.len, ipaddr.bs, serverport);
    StringFree(&ipaddr);

    NetNode server = NetNodeNew(serverfd);
    NetSelectCtx ctx;
    NetInit(&ctx, serverfd);

    clientstate = CONNECTED;

    // Send Client Info message to server
    u8 typeid = 1;
    char *alias = "rob";
    server.seq++;
    NetPackMsg(&server.writebuf, "%b%w%s", typeid, server.seq, alias);
    z = NetSend2(serverfd, &server.writebuf, &ctx);
    if (z == 0)
        clientstate = WAITING_CLIENT_INFO_ACK;

    fd_set tmp_readfds, tmp_writefds;
    while (1) {
        tmp_readfds = ctx.readfds;
        tmp_writefds = ctx.writefds;
        z = select(ctx.maxfd+1, &tmp_readfds, &tmp_writefds, NULL, NULL);
        if (z == 0) // timeout
            continue;
        if (z == -1 && errno == EINTR)
            continue;
        if (z == -1) {
            fprintf(stderr, "select(): %s\n", strerror(errno));
            break;
        }

        int read_eof = 0;
        if (FD_ISSET(serverfd, &tmp_readfds)) {
            if (NetRecv(serverfd, &server.readbuf) == 0)
                read_eof = 1;
            while (1) {
                if (server.msglen == 0) {
                    if (server.readbuf.len >= sizeof(u16)) {
                        u16 *bs = (u16 *) server.readbuf.bs;
                        server.msglen = ntohs(*bs);
                        if (server.msglen == 0) {
                            read_eof = 1;
                            break;
                        }
                        BufferShift(&server.readbuf, sizeof(u16));
                        continue;
                    }
                    break;
                } else {
                    // Read msg body (msglen bytes)
                    if (server.readbuf.len >= server.msglen) {
                        server_sent_msg(&ctx, &server, server.readbuf.bs, server.msglen);
                        BufferShift(&server.readbuf, server.msglen);
                        server.msglen = 0;
                        continue;
                    }
                    break;
                }
            }
            if (read_eof) {
                server_end_transmission(&ctx, &server);
                FD_CLR(serverfd, &ctx.readfds);
                shutdown(serverfd, SHUT_RD);
                server.shut_rd = 1;

                // Close serverfd if no remaining reads and writes.
                if (server.writebuf.len == 0) {
                    FD_CLR(serverfd, &ctx.writefds);
                    shutdown(serverfd, SHUT_WR);
                    break;
                }
            }
        }
        if (FD_ISSET(serverfd, &tmp_writefds)) {
            z = NetSend2(serverfd, &server.writebuf, &ctx);
            if (z == 0)
                clientstate = WAITING_CLIENT_INFO_ACK;

            // Close serverfd if no remaining reads and writes.
            if (z == 0 && server.shut_rd) {
                shutdown(serverfd, SHUT_WR);
                break;
            }
        }
    }
    close(serverfd);

    return 0;
}

