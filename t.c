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

int RegisterUser(String alias, String pwd);
String password_hash(String phrase);
int password_verify(String phrase, String hash);

void on_host_connected(HostCtx *hostctx);
void on_host_recv_msg(HostCtx *hostctx, char *msgbytes, u16 len, fd_set *writefds, int *maxfd);
void on_host_eof(HostCtx *hostctx);

void initdb(char *dbfile);

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
void on_host_recv_msg(HostCtx *hostctx, char *msgbytes, u16 len, fd_set *writefds, int *maxfd) {
    int z;
    u8 msgno = MSGNO(msgbytes);
    if (msgno == 0)
        return;

    // Skip over msgno (first byte)
    msgbytes++;
    len--;

    if (msgno == REGISTERMSG) {
        String alias = StringNew0();
        String pwd = StringNew0();
        NetUnpack(msgbytes, len, "%s%s", &alias, &pwd);
        printf("** REGISTERMSG alias: '%.*s' pwd: '%.*s' **\n", alias.len, alias.bs, pwd.len, pwd.bs);

        String tok = StringNew0();
        String zstatus = StringNew0();

        z = RegisterUser(alias, pwd);
        if (z == 0) {
            // Generate token from alias + pwd
            String s = StringDup(alias);
            StringAppend(&s, pwd.bs);
            String hash = password_hash(s);

            StringAssign(&tok, hash.bs);
            StringAssign(&zstatus, "OK");

            StringFree(&s);
            StringFree(&hash);
        } else if (z == -1) {
            StringAssign(&tok, "");
            StringAssignFormat(&zstatus, "Alias '%s' already taken", alias.bs);
        } else {
            StringAssign(&tok, "");
            StringAssign(&zstatus, "Error creating new user");
        }

        NetPackLen(&hostctx->writebuf, "%b%s%b%s", LOGINRESPMSG, tok.bs, z, zstatus.bs);
        NetSend2(hostctx->fd, &hostctx->writebuf, writefds, maxfd);

        StringFree(&alias);
        StringFree(&pwd);
        StringFree(&tok);
        StringFree(&zstatus);
    }
}
void on_host_eof(HostCtx *hostctx) {
    fprintf(stderr, "Client %d end transmission\n", hostctx->fd);
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

#define CRYPTSALT "salt1234567890"

String password_hash(String phrase) {
    if (phrase.len > CRYPT_MAX_PASSPHRASE_SIZE)
        phrase.bs[CRYPT_MAX_PASSPHRASE_SIZE] = 0;

    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    char *pz = crypt_r(phrase.bs, CRYPTSALT, &data);
    assert(pz != NULL);

    return StringNew(data.output);
}
int password_verify(String phrase, String hash) {
    if (phrase.len > CRYPT_MAX_PASSPHRASE_SIZE)
        phrase.bs[CRYPT_MAX_PASSPHRASE_SIZE] = 0;

    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    char *pz = crypt_r(phrase.bs, CRYPTSALT, &data);
    assert(pz != NULL);

    return StringEquals(hash, data.output);
}

void initdb(char *dbfile) {
    char *s;
    char *errstr=NULL;
    int z = sqlite3_open(dbfile, &db);
    if (z != 0)
        panic((char *) sqlite3_errmsg(db));

    s = "CREATE TABLE IF NOT EXISTS user (userid INTEGER PRIMARY KEY NOT NULL, alias TEXT NOT NULL, password TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS msg (msgid INTEGER PRIMARY KEY NOT NULL, date INTEGER, text TEXT NOT NULL, userid_from INTEGER NOT NULL, userid_to INTEGER NOT NULL);";
    z = sqlite3_exec(db, s, 0, 0, &errstr);
    if (z != 0)
        panic(errstr);
}

int RegisterUser(String alias, String pwd) {
    char *s;
    sqlite3_stmt *stmt;

    // Return error if user alias already exists.
    s = "SELECT userid FROM user WHERE alias = ?";
    sqlite3_prepare_v2(db, s, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, alias.bs, -1, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    // Create new user
    String pwdhash = password_hash(pwd);
    s = "INSERT INTO user (alias, password) VALUES (?, ?);";
    sqlite3_prepare_v2(db, s, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, alias.bs, -1, NULL);
    sqlite3_bind_text(stmt, 2, pwdhash.bs, -1, NULL);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        StringFree(&pwdhash);
        return -2;
    }

    sqlite3_finalize(stmt);
    StringFree(&pwdhash);
    return 0;
}

