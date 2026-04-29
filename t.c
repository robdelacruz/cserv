#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <crypt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "sqlite3.h"
#include "clib.h"
#include "cnet.h"
#include "msg.h"

#define DBFILE "cserve.db"

HostCtx *FindHostCtxByFd(Array a, int fd);
HostCtx *FindHostCtxByAlias(Array a, char *alias);
void RemoveHostCtxByFd(Array *a, int fd);

String password_hash(String phrase);
int password_verify(String phrase, String hash);

void initdb(char *dbfile);
void generate_token(String alias, String pwd, String *tok);
int RegisterUser(String alias, String pwd, String *tok);
int LoginUser(String alias, String pwd, String *tok);

void on_host_connected(HostCtx *hostctx);
void on_host_recv_msg(HostCtx *hostctx, char *msgbytes, u16 len, fd_set *writefds, int *maxfd);
void on_host_eof(HostCtx *hostctx);

sqlite3 *db;
Array hostctxs;

void sigint(int sig) {
    printf("\nTerminating Server.\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    int z;
    char *host = "localhost";
    char *port = "8000";
    fd_set readfds, writefds;
    fd_set readfds0, writefds0;
    int maxfd=0;

    if (argc > 1)
        host = argv[1];
    if (argc > 2)
        port = argv[2];

    signal(SIGINT, sigint);

    initdb(DBFILE);

    int backlog = 50;
    struct sockaddr sa;
    int s0 = OpenListenSocket(host, port, backlog, &sa);
    if (s0 == -1)
        exit(1);

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(s0, &readfds);
    maxfd = s0;
    hostctxs = ArrayNew(255, sizeof(HostCtx), (FreeFunc) HostCtxFree);

    String ipaddr = StringNew("");
    GetTextIPAddress(&sa, &ipaddr);
    printf("Listening on %.*s port %s...\n", ipaddr.len, ipaddr.bs, port);
    StringFree(&ipaddr);

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

        for (int i=0; i <= maxfd; i++) {
            if (FD_ISSET(i, &readfds0)) {
                // New client connection, ready to receive data from client.
                if (i == s0) {
                    socklen_t sa_len = sizeof(struct sockaddr_in);
                    struct sockaddr_in sa;
                    int clientfd = accept(s0, (struct sockaddr *) &sa, &sa_len);
                    if (clientfd == -1) {
                        fprintf(stderr, "accept(): %s\n", strerror(errno));
                        continue;
                    }
                    FD_SET(clientfd, &readfds);
                    if (clientfd > maxfd)
                        maxfd = clientfd;

                    HostCtx hostctx = HostCtxNew(clientfd);
                    ArrayAppend(&hostctxs, &hostctx);
                    on_host_connected(&hostctx);
                } else {
                    int clientfd = i;

                    HostCtx *hostctx = FindHostCtxByFd(hostctxs, clientfd);
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
                                on_host_recv_msg(hostctx, readbuf->bs, hostctx->msglen, &writefds, &maxfd);
                                BufferShift(readbuf, hostctx->msglen);
                                hostctx->msglen = 0;
                                continue;
                            }
                            break;
                        }
                    }
                    if (read_eof) {
                        on_host_eof(hostctx);
                        FD_CLR(clientfd, &readfds);
                        shutdown(clientfd, SHUT_RD);
                        hostctx->shut_rd = 1;

                        // Remove hostctx if no remaining reads and writes.
                        if (hostctx->writebuf.len == 0) {
                            RemoveHostCtxByFd(&hostctxs, clientfd);
                            FD_CLR(clientfd, &writefds);
                            shutdown(clientfd, SHUT_WR);
                            close(clientfd);
                        }
                    }
                }
            }
            if (FD_ISSET(i, &writefds0)) {
                int clientfd = i;

                HostCtx *hostctx = FindHostCtxByFd(hostctxs, clientfd);
                if (hostctx == NULL) {
                    fprintf(stderr, "Can't find client buffer %d\n", clientfd);
                    continue;
                }

                NetSend2(clientfd, &hostctx->writebuf, &writefds, &maxfd);

                // Remove hostctx if no remaining reads and writes.
                if (hostctx->writebuf.len == 0 && hostctx->shut_rd) {
                    RemoveHostCtxByFd(&hostctxs, clientfd);
                    shutdown(clientfd, SHUT_WR);
                    close(clientfd);
                }
            }
        }
    }

    close(s0);
    ArrayFree(&hostctxs);
    return 0;
}

void on_host_connected(HostCtx *hostctx) {
    fprintf(stderr, "Connected to client %d\n", hostctx->fd);
}
void on_host_eof(HostCtx *hostctx) {
    fprintf(stderr, "Client %d end transmission\n", hostctx->fd);
}

void on_host_recv_msg(HostCtx *hostctx, char *msgbytes, u16 len, fd_set *writefds, int *maxfd) {
    int z;
    u8 msgno = MSGNO(msgbytes);
    if (msgno == 0)
        return;

    // Skip over msgno (first byte)
    msgbytes++;
    len--;

    if (msgno == REGISTER_REQUEST) {
        String alias = StringNew0();
        String pwd = StringNew0();
        String tok = StringNew0();
        String zstatus = StringNew0();

        NetUnpack(msgbytes, len, "%s%s", &alias, &pwd);
        printf("** REGISTER_REQUEST alias: '%.*s' pwd: '%.*s' **\n", alias.len, alias.bs, pwd.len, pwd.bs);

        z = RegisterUser(alias, pwd, &tok);
        if (z == 0)
            StringAssign(&zstatus, "OK");
        else if (z == -1)
            StringAssignFormat(&zstatus, "Alias '%s' already taken", alias.bs);
        else
            StringAssign(&zstatus, "Error creating new user");

        NetPackLen(&hostctx->writebuf, "%b%s%b%s", LOGIN_RESPONSE, tok.bs, z, zstatus.bs);
        NetSend2(hostctx->fd, &hostctx->writebuf, writefds, maxfd);

        StringFree(&alias);
        StringFree(&pwd);
        StringFree(&tok);
        StringFree(&zstatus);
    } else if (msgno == LOGIN_REQUEST) {
        String alias = StringNew0();
        String pwd = StringNew0();
        String tok = StringNew0();
        String zstatus = StringNew0();

        NetUnpack(msgbytes, len, "%s%s", &alias, &pwd);
        printf("** LOGIN_REQUEST alias: '%.*s' pwd: '%.*s' **\n", alias.len, alias.bs, pwd.len, pwd.bs);

        z = LoginUser(alias, pwd, &tok);
        if (z == 0)
            StringAssign(&zstatus, "OK");
        else if (z == -1)
            StringAssign(&zstatus, "User doesn't exist");
        else
            StringAssign(&zstatus, "Login incorrect");

        NetPackLen(&hostctx->writebuf, "%b%s%b%s", LOGIN_RESPONSE, tok.bs, z, zstatus.bs);
        NetSend2(hostctx->fd, &hostctx->writebuf, writefds, maxfd);

        StringFree(&alias);
        StringFree(&pwd);
        StringFree(&tok);
        StringFree(&zstatus);
    }
}

HostCtx *FindHostCtxByFd(Array a, int fd) {
    HostCtx *hostctxs = (HostCtx *) a.items;
    for (int i=0; i < a.len; i++) {
        if (hostctxs[i].fd == fd)
            return &hostctxs[i];
    }
    return NULL;
}
HostCtx *FindHostCtxByAlias(Array a, char *alias) {
    HostCtx *hostctxs = (HostCtx *) a.items;
    for (int i=0; i < a.len; i++) {
        if (StringEquals(hostctxs[i].alias, alias))
            return &hostctxs[i];
    }
    return NULL;
}
void RemoveHostCtxByFd(Array *a, int fd) {
    HostCtx *hostctxs = (HostCtx *) a->items;
    for (int i=0; i < a->len; i++) {
        if (hostctxs[i].fd == fd) {
            ArrayRemove(a, i);
            return;
        }
    }
}

