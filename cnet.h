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
} NetNode;

typedef struct {
    NetNode *items;
    u16 len;
    u16 cap;
} NetNodeArray;

typedef struct {
    fd_set readfds;
    fd_set writefds;
    int maxfd;
    NetNodeArray nodes;
} NetSelectCtx;

void NetInit(NetSelectCtx *ctx, int serverfd);

int OpenListenSocket(char *host, char *port, int backlog, struct sockaddr *sa);
int OpenConnectSocket(char *host, char *port, int backlog, struct sockaddr *sa);
void GetTextIPAddress(struct sockaddr *sa, String *dest);
int NetRecv(int fd, Buffer *buf);
int NetSend(int fd, Buffer *buf);
int NetSend2(int fd, Buffer *buf, NetSelectCtx *ctx);
int NetPack(Buffer *buf, char *fmt, ...);
int NetPackLen(Buffer *buf, char *fmt, ...);
void NetUnpack(char *bs, int bslen, char *fmt, ...);

NetNode NetNodeNew(int fd);
void NetNodeFree(NetNode *n);
NetNodeArray NetNodeArrayNew(u16 cap);
void NetNodeArrayFree(NetNodeArray *a);
void NetNodeArrayClear(NetNodeArray *a);
void NetNodeArrayAppend(NetNodeArray *a, NetNode n);
void NetNodeArrayRemove(NetNodeArray *a, int fd);
NetNode *NetNodeArrayFind(NetNodeArray a, int fd);
NetNode *NetNodeArrayFindAlias(NetNodeArray a, char *alias);

#endif
