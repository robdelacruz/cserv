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

// Client states
#define OFFLINE 0
#define CONNECTED 1
#define WAITING_SERVER_HELLO 2
#define SERVER_READY 3
#define WAITING_SERVER_RESPONSE 4

int clientstate = 0;

void server_sent_block(int serverfd, char *blk, u16 blk_len, Buffer *writebuf) {
    u8 typeid = *((u8 *) blk);
    printf("Server %d received typeid: %d\n", serverfd, (int) typeid);

    if (clientstate == WAITING_SERVER_HELLO) {
        if (typeid == 1) {
            String name = StringNew("");
            NetUnpack(blk, blk_len, "%b%s", &typeid, &name);
            printf("Server '%.*s' responded hello\n", name.len, name.bs);
            StringFree(&name);
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

    clientstate = CONNECTED;

    // Send Hello message to server
    u8 typeid = 1;
    char *alias = "rob";
    NetPackBlock(&writebuf, "%b%s", typeid, alias);
    z = NetSend(serverfd, &writebuf);
    if (z == 0)
        clientstate = WAITING_SERVER_HELLO;
    else
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
            if (z == 0) {
                FD_CLR(serverfd, &writefds);
                clientstate = WAITING_SERVER_HELLO;
            } else if (z == 1) {
                continue;
            } else {
                fprintf(stderr, "Failed to send to server.\n");
                continue;
            }

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

