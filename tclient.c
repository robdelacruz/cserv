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

void on_host_recv_msg(SelectCtx *selectctx, HostCtx *hostctx, char *msgbytes, u16 len) {
    Msg msg;
    MsgUnpack(&msg, msgbytes, len);
    u8 msgno = MSGNO(msgbytes);
    if (msgno == 0)
        return;
    MsgPrint(&msg);
    MsgFree(&msg);
}
void on_host_eof(SelectCtx *selectctx, HostCtx *hostctx) {
    fprintf(stderr, "Server %d end transmission\n", hostctx->fd);
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
    StringFree(ipaddr);

    HostCtx hostctx = HostCtxNew(serverfd);
    SelectCtx selectctx = SelectCtxNew(serverfd);

    // Try sending some message to server
    u8 msgno = REGISTERMSG;
    NetPackLen(&hostctx.writebuf, "%b%s%s", msgno, "abcuser3", "abc123");
    z = NetSend2(serverfd, &hostctx.writebuf, &selectctx);

    fd_set tmp_readfds, tmp_writefds;
    while (1) {
        tmp_readfds = selectctx.readfds;
        tmp_writefds = selectctx.writefds;
        z = select(selectctx.maxfd+1, &tmp_readfds, &tmp_writefds, NULL, NULL);
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
            if (NetRecv(serverfd, &hostctx.readbuf) == 0)
                read_eof = 1;

            // Each message is a 16bit msglen value followed by msglen sequence of bytes.
            // A msglen of 0 means no more bytes remaining in the stream.

            Buffer *readbuf = &hostctx.readbuf;
            while (1) {
                if (hostctx.msglen == 0) {
                    if (readbuf->len >= sizeof(u16)) {
                        u16 *bs = (u16 *) readbuf->bs;
                        hostctx.msglen = ntohs(*bs);
                        if (hostctx.msglen == 0) {
                            read_eof = 1;
                            break;
                        }
                        BufferShift(readbuf, sizeof(u16));
                        continue;
                    }
                    break;
                } else {
                    // Read msg body (msglen bytes)
                    if (readbuf->len >= hostctx.msglen) {
                        on_host_recv_msg(&selectctx, &hostctx, readbuf->bs, hostctx.msglen);
                        BufferShift(readbuf, hostctx.msglen);
                        hostctx.msglen = 0;
                        continue;
                    }
                    break;
                }
            }
            if (read_eof) {
                on_host_eof(&selectctx, &hostctx);
                FD_CLR(serverfd, &selectctx.readfds);
                shutdown(serverfd, SHUT_RD);
                hostctx.shut_rd = 1;

                // Close serverfd if no remaining reads and writes.
                if (hostctx.writebuf.len == 0) {
                    FD_CLR(serverfd, &selectctx.writefds);
                    shutdown(serverfd, SHUT_WR);
                    break;
                }
            }
        }
        if (FD_ISSET(serverfd, &tmp_writefds)) {
            z = NetSend2(serverfd, &hostctx.writebuf, &selectctx);

            // Close serverfd if no remaining reads and writes.
            if (z == 0 && hostctx.shut_rd) {
                shutdown(serverfd, SHUT_WR);
                break;
            }
        }
    }
    close(serverfd);

    SelectCtxFree(&selectctx);
    return 0;
}

