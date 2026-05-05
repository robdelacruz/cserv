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

void on_host_recv_msg(HostCtx *hostctx, char *msgbytes, u16 len, fd_set *writefds, int *maxfd) {
    int z;
    u8 msgno = MSGNO(msgbytes);
    printf("on_host_recv_msg() msgno: %d\n", msgno);
    if (msgno == 0)
        return;

    // Skip over msgno (first byte)
    msgbytes++;
    len--;

    if (msgno == LOGINUSER_RESPONSE) {
        String tok = StringNew0();
        i8 retno;
        String errorstr = StringNew0();

        NetUnpack(msgbytes, len, "%s%b%s", &tok, &retno, &errorstr);
        printf("** LOGINUSER_RESPONSE tok: '%.*s' retno: %d errorstr: '%.*s' **\n", tok.len, tok.bs, retno, errorstr.len, errorstr.bs);

        StringFree(&tok);
        StringFree(&errorstr);
    }
}
void on_host_eof(HostCtx *hostctx) {
    fprintf(stderr, "Server %d end transmission\n", hostctx->fd);
}

int main(int argc, char *argv[]) {
    int z;
    char *serverhost = "localhost";
    char *serverport = "8000";
    fd_set readfds, writefds;
    fd_set readfds0, writefds0;
    int maxfd=0;

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

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(serverfd, &readfds);
    maxfd = serverfd;
    HostCtx hostctx = HostCtxNew(serverfd);

    String ipaddr = StringNew("");
    GetTextIPAddress(&sa, &ipaddr);
    printf("Connected to %.*s port %s...\n", ipaddr.len, ipaddr.bs, serverport);
    StringFree(&ipaddr);

    // Try sending some message to server
    u8 msgno = REGISTERUSER_REQUEST;
    NetPackLen(&hostctx.writebuf, "%b%s%s", msgno, "user6", "abc");
    z = NetSend2(serverfd, &hostctx.writebuf, &writefds, &maxfd);

//    u8 msgno = LOGINUSER_REQUEST;
//    NetPackLen(&hostctx.writebuf, "%b%s%s", msgno, "robtwister", "password123");
//    z = NetSend2(serverfd, &hostctx.writebuf, &writefds, &maxfd);

    while (1) {
        readfds0 = readfds;
        writefds0 = writefds;
        z = select(maxfd+1, &readfds0, &writefds0, NULL, NULL);
        if (z == 0) // timeout
            continue;
        if (z == -1 && errno == EINTR)
            continue;
        if (z == -1) {
            fprintf(stderr, "select(): %s\n", strerror(errno));
            break;
        }

        int read_eof = 0;
        if (FD_ISSET(serverfd, &readfds0)) {
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
                        on_host_recv_msg(&hostctx, readbuf->bs, hostctx.msglen, &writefds, &maxfd);
                        BufferShift(readbuf, hostctx.msglen);
                        hostctx.msglen = 0;
                        continue;
                    }
                    break;
                }
            }
            if (read_eof) {
                on_host_eof(&hostctx);
                FD_CLR(serverfd, &readfds);
                shutdown(serverfd, SHUT_RD);
                hostctx.shut_rd = 1;

                // Close serverfd if no remaining reads and writes.
                if (hostctx.writebuf.len == 0) {
                    FD_CLR(serverfd, &writefds);
                    shutdown(serverfd, SHUT_WR);
                    break;
                }
            }
        }
        if (FD_ISSET(serverfd, &writefds0)) {
            z = NetSend2(serverfd, &hostctx.writebuf, &writefds, &maxfd);

            // Close serverfd if no remaining reads and writes.
            if (z == 0 && hostctx.shut_rd) {
                shutdown(serverfd, SHUT_WR);
                break;
            }
        }
    }

    close(serverfd);
    return 0;
}

