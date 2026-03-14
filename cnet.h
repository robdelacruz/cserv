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

typedef struct _Client {
    int fd;
    Buffer readbuf;
    Buffer writebuf;
    u16 blk_len;
    int shut_rd;
    int shut_wr;
} Client;

typedef struct {
    Client *items;
    u16 len;
    u16 cap;
    i8 isfreeitems;
} ClientArray;

int OpenListenSocket(char *host, char *port, int backlog, struct sockaddr *sa);
int OpenConnectSocket(char *host, char *port, int backlog, struct sockaddr *sa);
void GetTextIPAddress(struct sockaddr *sa, String *dest);
int NetRecv(int fd, Buffer *buf);
int NetSend(int fd, Buffer *buf);
void NetPack(Buffer *buf, char *fmt, ...);
void NetUnpack(char *blk, int blklen, char *fmt, ...);

Client ClientNew(int fd);
void ClientFree(Client *client);
ClientArray ClientArrayNew(u16 cap);
void ClientArrayFree(ClientArray *ca);
void ClientArrayClear(ClientArray *ca);
void ClientArrayAppend(ClientArray *ca, Client client);
void ClientArrayRemove(ClientArray *ca, int fd);
Client *ClientArrayFind(ClientArray *ca, int fd);

#endif
