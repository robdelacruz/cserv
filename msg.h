#ifndef MSG_H
#define MSG_H

#include "clib.h"
#include "cnet.h"

#define REGISTERMSG 1
#define LOGINMSG 2
#define COMMANDMSG 3
#define ALIASESMSG 4

#define MSGNO(bs) (*((u8 *)bs))

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
void free_message(void *msg);

#endif
