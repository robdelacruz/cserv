#ifndef MSG_H
#define MSG_H

#include "clib.h"
#include "cnet.h"

#define MSGID_CLIENTINFO 1
#define MSGID_ACK 2
#define MSGID_COMMAND 3
#define MSGID_CHAT 4

#define MSGID_FROM_MSGBYTES(msgbytes) (*((u8 *)msgbytes))
#define MSGID_FROM_STRUCT(msgstruct) (((BlankMsg *) msg)->msgid)

typedef struct {
    u8 msgid;
    u16 seq;
} BlankMsg;

typedef struct {
    u8 msgid;
    u16 seq;
    String alias;
} ClientInfoMsg;

typedef struct {
    u8 msgid;
    u16 seq;
    String acktext;
} AckMsg;

typedef struct {
    u8 msgid;
    u16 seq;
    String command;
} CommandMsg;

typedef struct {
    u8 msgid;
    u16 seq;
    String from_alias;
    String to_alias;
    String text;
} ChatMsg;

void print_msg(char *msgbytes, u16 len);
void *unpack_message(char *msgbytes, u16 len);
void free_message(void *msg);

#endif
