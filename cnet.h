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
    Buffer readbuf;
    Buffer writebuf;
    u16 blk_len;
    int shut_rd;
    int shut_wr;
    String alias;
} NetNode;

typedef struct {
    NetNode *items;
    u16 len;
    u16 cap;
} NetNodeArray;

int OpenListenSocket(char *host, char *port, int backlog, struct sockaddr *sa);
int OpenConnectSocket(char *host, char *port, int backlog, struct sockaddr *sa);
void GetTextIPAddress(struct sockaddr *sa, String *dest);
int NetRecv(int fd, Buffer *buf);
int NetSend(int fd, Buffer *buf);
int NetPack(Buffer *buf, char *fmt, ...);
int NetPackBlock(Buffer *buf, char *fmt, ...);
void NetUnpack(char *blk, int blklen, char *fmt, ...);

NetNode NetNodeNew(int fd);
void NetNodeFree(NetNode *n);
NetNodeArray NetNodeArrayNew(u16 cap);
void NetNodeArrayFree(NetNodeArray *na);
void NetNodeArrayClear(NetNodeArray *na);
void NetNodeArrayAppend(NetNodeArray *na, NetNode n);
void NetNodeArrayRemove(NetNodeArray *na, int fd);
NetNode *NetNodeArrayFind(NetNodeArray na, int fd);
NetNode *NetNodeArrayFindAlias(NetNodeArray na, char *alias);

#endif
