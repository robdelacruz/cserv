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

    NetSelectCtx ctx;
    NetInit(&ctx, serverfd);
    Buffer readbuf = BufferNew(4096);
    Buffer writebuf = BufferNew(4096);
    u16 msglen = 0;
    int shut_rd = 0;

    clientstate = CONNECTED;
    u16 clientseq = 0;

    // Send Client Info message to server
    u8 typeid = 1;
    char *alias = "rob";
    clientseq++;
    NetPackMsg(&writebuf, "%b%w%s", typeid, clientseq, alias);
    z = NetSend(serverfd, &writebuf);
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
            if (NetRecv(serverfd, &readbuf) == 0)
                read_eof = 1;
            while (1) {
                if (msglen == 0) {
                    if (readbuf.len >= sizeof(u16)) {
                        u16 *bs = (u16 *) readbuf.bs;
                        msglen = ntohs(*bs);
                        if (msglen == 0) {
                            read_eof = 1;
                            break;
                        }
                        BufferShift(&readbuf, sizeof(u16));
                        continue;
                    }
                    break;
                } else {
                    // Read msg body (msglen bytes)
                    if (readbuf.len >= msglen) {
                        u16 writebuf_org_len = writebuf.len;
                        server_sent_msg(serverfd, readbuf.bs, msglen, &writebuf);
                        BufferShift(&readbuf, msglen);
                        msglen = 0;
                        continue;
                    }
                    break;
                }
            }
            if (read_eof) {
                server_end_transmission(serverfd);
                FD_CLR(serverfd, &ctx.readfds);
                shutdown(serverfd, SHUT_RD);
                shut_rd = 1;

                // Close serverfd if no remaining reads and writes.
                if (writebuf.len == 0) {
                    FD_CLR(serverfd, &ctx.writefds);
                    shutdown(serverfd, SHUT_WR);
                    break;
                }
            }
        }
        if (FD_ISSET(serverfd, &tmp_writefds)) {
            printf("FD_ISSET()...\n");
            z = NetSend(serverfd, &writebuf);
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
            if (writebuf.len == 0 && shut_rd) {
                FD_CLR(serverfd, &ctx.writefds);
                shutdown(serverfd, SHUT_WR);
                break;
            }
        }
    }
    close(serverfd);

    return 0;
}

