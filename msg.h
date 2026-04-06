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

// This is just used as a placeholder struct to
// reserve enough bytes to fit the largest msg.
// Msg should be type cast to the appropriate msg
// struct before it's used.
typedef struct {
    u8 msgno;
    char _tmp[255];
} Msg;

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

void MsgFree(void *msg);
void MsgPrint(void *msg);
void MsgPack(void *msg, Buffer *buf);
void MsgUnpack(Msg *msg, char *msgbytes, u16 len);

#endif
