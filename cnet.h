#ifndef CNET_H
#define CNET_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clib.h"

typedef struct {
    int fd;
    u16 seq;
    Buffer readbuf;
    Buffer writebuf;
    u16 msglen;
    int shut_rd;
    int shut_wr;
    String alias;
} HostCtx;

typedef struct {
    HostCtx *items;
    u16 len;
    u16 cap;
} HostCtxArray;

typedef struct {
    fd_set readfds;
    fd_set writefds;
    int maxfd;
    HostCtxArray hostctxs;
} SelectCtx;

void NetInit(SelectCtx *selectctx, int serverfd);

int CreateNonBlockingSocket(char *host, char *port, struct sockaddr *sa);
int OpenListenSocket(char *host, char *port, int backlog, struct sockaddr *sa);
int OpenConnectSocket(char *host, char *port, int backlog, struct sockaddr *sa);
void GetTextIPAddress(struct sockaddr *sa, String *dest);
int NetRecv(int fd, Buffer *buf);
int NetSend(int fd, Buffer *buf);
int NetSend2(int fd, Buffer *buf, SelectCtx *selectctx);
int NetPack(Buffer *buf, char *fmt, ...);
int NetPackLen(Buffer *buf, char *fmt, ...);
void NetUnpack(char *bs, int bslen, char *fmt, ...);

HostCtx HostCtxNew(int fd);
void HostCtxFree(HostCtx *hostctx);

HostCtxArray HostCtxArrayNew(u16 cap);
void HostCtxArrayFree(HostCtxArray *a);
void HostCtxArrayClear(HostCtxArray *a);
void HostCtxArrayAppend(HostCtxArray *a, HostCtx hostctx);
void HostCtxArrayRemove(HostCtxArray *a, int fd);
HostCtx *HostCtxArrayFind(HostCtxArray a, int fd);
HostCtx *HostCtxArrayFindAlias(HostCtxArray a, char *alias);

#endif
