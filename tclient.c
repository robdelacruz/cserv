#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clib.h"
#include "cnet.h"

ssize_t sendbytes(int sock, char *buf, size_t count);
ssize_t recvbytes(int sock, char *buf, size_t count);
void sendTextMessage(int sock, char *msg);
void sendZeroMessage(int sock);

void server_sent_block(int serverfd, char *blk, u16 blk_len, Buffer *writebuf) {
    printf("Server %d received block '%.*s'\n", serverfd, blk_len, blk);
}
void server_end_transmission(int serverfd) {
    fprintf(stderr, "Server %d end transmission\n", serverfd);
}

int main(int argc, char *argv[]) {
    int z;
    char *serverhost = "localhost";
    char *serverport = "8000";

    int backlog = 50;
    struct sockaddr sa;
    int serverfd = OpenConnectSocket(serverhost, serverport, backlog, &sa);
    if (serverfd == -1)
        exit(1);

    String ipaddr = StringNew("");
    GetTextIPAddress(&sa, &ipaddr);
    printf("Connected to %.*s port %s...\n", ipaddr.len, ipaddr.bs, serverport);
    StringFree(&ipaddr);

    fd_set readfds, writefds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(serverfd, &readfds);

    Buffer readbuf = BufferNew(4096);
    Buffer writebuf = BufferNew(4096);
    int blk_len = 0;
    int shut_rd = 0;

    // Queue up Client Intro message
    u16 msglen = 0;
    u8 typeid = 1;
    char *alias = "rob";
    u16 *pmsglen = (u16 *) (writebuf.bs + writebuf.len);
    int oldlen = writebuf.len;
    NetPack(&writebuf, "%w%b%s", msglen, typeid, alias);
    *pmsglen = htons((u16) (writebuf.len - oldlen - sizeof(u16)));
    z = NetSend(serverfd, &writebuf);
    if (z == 1)
        FD_SET(serverfd, &writefds);

    fd_set tmp_readfds, tmp_writefds;
    while (1) {
        tmp_readfds = readfds;
        tmp_writefds = writefds;
        z = select(serverfd+1, &tmp_readfds, &tmp_writefds, NULL, NULL);
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
                if (blk_len == 0) {
                    if (readbuf.len >= sizeof(u16)) {
                        u16 *bs = (u16 *) readbuf.bs;
                        blk_len = ntohs(*bs);
                        if (blk_len == 0) {
                            read_eof = 1;
                            break;
                        }
                        BufferShift(&readbuf, sizeof(u16));
                        continue;
                    }
                    break;
                } else {
                    // Read block body (blk_len bytes)
                    if (readbuf.len >= blk_len) {
                        u16 writebuf_org_len = writebuf.len;
                        server_sent_block(serverfd, readbuf.bs, blk_len, &writebuf);
                        BufferShift(&readbuf, blk_len);
                        blk_len = 0;

                        // writebuf contains response, if any
                        if (writebuf.len > writebuf_org_len)
                            FD_SET(serverfd, &writefds);
                        continue;
                    }
                    break;
                }
            }
            if (read_eof) {
                server_end_transmission(serverfd);
                FD_CLR(serverfd, &readfds);
                shutdown(serverfd, SHUT_RD);
                shut_rd = 1;

                // Close serverfd if no remaining reads and writes.
                if (writebuf.len == 0) {
                    FD_CLR(serverfd, &writefds);
                    shutdown(serverfd, SHUT_WR);
                    break;
                }
            }
        }
        if (FD_ISSET(serverfd, &tmp_writefds)) {
            printf("FD_ISSET()...\n");
            z = NetSend(serverfd, &writebuf);
            if (z == 0)
                FD_CLR(serverfd, &writefds);

            // Close serverfd if no remaining reads and writes.
            if (writebuf.len == 0 && shut_rd) {
                FD_CLR(serverfd, &writefds);
                shutdown(serverfd, SHUT_WR);
                break;
            }
        }
    }
    close(serverfd);

    return 0;
}

void sendTextMessage(int sock, char *msg) {
    int z;
    unsigned short msglen = strlen(msg);
    unsigned short netmsglen = htons(msglen);
    z = sendbytes(sock, (char *) &netmsglen, sizeof(netmsglen));
    if (z == -1)
        fprintf(stderr, "send(): %s\n", strerror(errno));
    z = sendbytes(sock, msg, strlen(msg));
    if (z == -1)
        fprintf(stderr, "send(): %s\n", strerror(errno));
}

void sendZeroMessage(int sock) {
    unsigned short msglen = 0;
    int z = sendbytes(sock, (char *) &msglen, sizeof(msglen));
    if (z == -1)
        fprintf(stderr, "send(): %s\n", strerror(errno));
}

// Send count buf bytes into sock.
// Returns num bytes sent or -1 for error
ssize_t sendbytes(int sock, char *buf, size_t count) {
    int nsent = 0;
    while (nsent < count) {
        int z = send(sock, buf+nsent, count-nsent, 0);
        // socket closed, no more data
        if (z == 0) {
            // socket closed
            break;
        }
        // interrupt occured during send, retry send.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        // no data available at the moment, just return what we have.
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        // any other error
        if (z == -1) {
            return z;
        }
        nsent += z;
    }

    return nsent;
}

// Receive count bytes into buf.
// Returns num bytes received or -1 for error.
ssize_t recvbytes(int sock, char *buf, size_t count) {
    memset(buf, '*', count); // initialize for debugging purposes.

    int nread = 0;
    while (nread < count) {
        int z = recv(sock, buf+nread, count-nread, 0);
        // socket closed, no more data
        if (z == 0) {
            break;
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        // no data available at the moment, just return what we have.
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        // any other error
        if (z == -1) {
            return z;
        }
        nread += z;
    }

    return nread;
}

