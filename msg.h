#ifndef MSG_H
#define MSG_H

#include "clib.h"
#include "cnet.h"

#define STATUSMSG 1
#define REGISTERMSG 2
#define LOGINMSG 3
#define COMMANDMSG 4
#define ALIASESMSG 5

#define MSGNO(bs) (*((u8 *)bs))

typedef struct {
    u8 msgno;
    u8 statusno;
    String statustext;
} StatusMsg;

typedef struct {
    u8 msgno;
    String alias;
    String pwd;
} RegisterMsg;

typedef struct {
    u8 msgno;
    String alias;
    String pwd;
} LoginMsg;

typedef struct {
    u8 msgno;
    String command;
} CommandMsg;

typedef struct {
    u8 msgno;
    String aliases;
} AliasesMsg;

void print_message(void *msg);
void *unpack_message(char *msgbytes, u16 len);
void pack_message(void *msg, Buffer *buf);
void free_message(void *msg);

#endif
