#include <stdio.h>
#include <stdlib.h>

#include "clib.h"
#include "msg.h"

void MsgFree(void *msg) {
    u8 msgno = MSGNO(msg);
    if (msgno == REGISTERMSG) {
        RegisterMsg *p = msg;
        StringFree(p->alias);
        StringFree(p->pwd);
    } else if (msgno == LOGINMSG) {
        LoginMsg *p = msg;
        StringFree(p->alias);
        StringFree(p->pwd);
    } else {
        fprintf(stderr, "MsgFree(): Unrecognized msgno %d\n", msgno);
    }
}

void MsgPrint(void *msg) {
    u8 msgno = MSGNO(msg);
    if (msgno == REGISTERMSG) {
        RegisterMsg *p = msg;
        printf("** Register alias: '%.*s' pwd: '%.*s' **\n", p->alias.len, p->alias.bs, p->pwd.len, p->pwd.bs);
    } else if (msgno == LOGINMSG) {
        LoginMsg *p = msg;
        printf("** Login alias: '%.*s' pwd: '%.*s' **\n", p->alias.len, p->alias.bs, p->pwd.len, p->pwd.bs);
    }
}

void MsgPack(void *msg, Buffer *buf) {
    u8 msgno = MSGNO(msg);
    if (msgno == REGISTERMSG) {
        RegisterMsg *p = msg;
        NetPackLen(buf, "%b%s%s", msgno, p->alias.bs, p->pwd.bs);
    } else if (msgno == LOGINMSG) {
        LoginMsg *p = msg;
        NetPackLen(buf, "%b%s%s", msgno, p->alias.bs, p->pwd.bs);
    } else {
        fprintf(stderr, "pack_message(): Unrecognized msgno %d\n", msgno);
    }
}

void MsgUnpack(Msg *msg, char *msgbytes, u16 len) {
    u8 msgno = MSGNO(msgbytes);
    if (msgno == REGISTERMSG) {
        RegisterMsg *p = (RegisterMsg *) msg;
        p->alias = StringNew("");
        p->pwd = StringNew("");
        NetUnpack(msgbytes, len, "%b%s%s", &p->msgno, &p->alias, &p->pwd);
    } else if (msgno == LOGINMSG) {
        LoginMsg *p = (LoginMsg *) msg;
        p->alias = StringNew("");
        p->pwd = StringNew("");
        NetUnpack(msgbytes, len, "%b%s%s", &p->msgno, &p->alias, &p->pwd);
    } else {
        msg->msgno = 0;
        fprintf(stderr, "unpack_message(): Unrecognized msgno %d\n", msgno);
    }
}

