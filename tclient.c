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

void sigint(int sig) {
    printf("\nTerminating Server.\n");
    exit(0);
}

// Client states
#define OFFLINE 0
#define CONNECTED 1
#define WAITING_SERVER_ACK 2
#define SERVER_READY 3
#define WAITING_SERVER_RESPONSE 4

int clientstate = 0;

void server_sent_msg(int serverfd, char *msg, u16 msglen, Buffer *writebuf) {
    u8 typeid = *((u8 *) msg);
    u16 seq = 0;
    printf("Server %d received typeid: %d\n", serverfd, (int) typeid);

    if (clientstate == WAITING_SERVER_ACK) {
        if (typeid == 2) {
            String ackstr = StringNew("");
            NetUnpack(msg, msglen, "%b%w%s", &typeid, &seq, &ackstr);
            printf("Server sent ack '%.*s'\n", ackstr.len, ackstr.bs);
            StringFree(&ackstr);
        }
    }
}
void server_end_transmission(int serverfd) {
    fprintf(stderr, "Server %d end transmission\n", serverfd);
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
    z = NetSend(serverfd, &server.writebuf);
    if (z == 0)
        clientstate = WAITING_SERVER_ACK;
    else
        FD_SET(serverfd, &ctx.writefds);

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
                        server_sent_msg(serverfd, server.readbuf.bs, server.msglen, &server.writebuf);
                        BufferShift(&server.readbuf, server.msglen);
                        server.msglen = 0;
                        continue;
                    }
                    break;
                }
            }
            if (read_eof) {
                server_end_transmission(serverfd);
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
            printf("FD_ISSET()...\n");
            z = NetSend(serverfd, &server.writebuf);
            if (z == 0) {
                FD_CLR(serverfd, &ctx.writefds);
                clientstate = WAITING_SERVER_ACK;
            } else if (z == 1) {
                continue;
            } else {
                fprintf(stderr, "Failed to send to server.\n");
                continue;
            }

            // Close serverfd if no remaining reads and writes.
            if (server.writebuf.len == 0 && server.shut_rd) {
                FD_CLR(serverfd, &ctx.writefds);
                shutdown(serverfd, SHUT_WR);
                break;
            }
        }
    }
    close(serverfd);

    return 0;
}

