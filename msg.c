#include <stdio.h>
#include <stdlib.h>

#include "clib.h"
#include "msg.h"

void MsgFree(void *msg) {
    u8 msgno = MSGNO(msg);
    if (msgno == STATUSMSG) {
        StatusMsg *p = msg;
        StringFree(p->statustext);
    } else if (msgno == REGISTERMSG) {
        RegisterMsg *p = msg;
        StringFree(p->alias);
        StringFree(p->pwd);
    } else if (msgno == LOGINMSG) {
        LoginMsg *p = msg;
        StringFree(p->alias);
        StringFree(p->pwd);
    } else if (msgno == COMMANDMSG) {
        CommandMsg *p = msg;
        StringFree(p->command);
    } else if (msgno == ALIASESMSG) {
        AliasesMsg *p = msg;
        StringFree(p->aliases);
    } else {
        fprintf(stderr, "MsgFree(): Unrecognized msgno %d\n", msgno);
    }
}

void MsgPrint(void *msg) {
    u8 msgno = MSGNO(msg);
    if (msgno == STATUSMSG) {
        StatusMsg *p = msg;
        printf("** Status statusno: %d statustext: '%.*s' **\n", p->statusno, p->statustext.len, p->statustext.bs);
    } else if (msgno == REGISTERMSG) {
        RegisterMsg *p = msg;
        printf("** Register alias: '%.*s' pwd: '%.*s' **\n", p->alias.len, p->alias.bs, p->pwd.len, p->pwd.bs);
    } else if (msgno == LOGINMSG) {
        LoginMsg *p = msg;
        printf("** Login alias: '%.*s' pwd: '%.*s' **\n", p->alias.len, p->alias.bs, p->pwd.len, p->pwd.bs);
    } else if (msgno == COMMANDMSG) {
        CommandMsg *p = msg;
        printf("** Command: '%.*s' **\n", p->command.len, p->command.bs);
    } else if (msgno == ALIASESMSG) {
        AliasesMsg *p = msg;
        StringList aliases = StringSplit(p->aliases, ";");
        printf("** Aliases: ");
        for (int i=0; i < aliases.len; i++) {
            String alias = aliases.items[i];
            printf("%.*s", alias.len, alias.bs);
            if (i < aliases.len-1)
                printf(", ");
        }
        printf(" **\n");
        StringListFree(aliases);
    }
}

void MsgPack(void *msg, Buffer *buf) {
    u8 msgno = MSGNO(msg);
    if (msgno == STATUSMSG) {
        StatusMsg *p = msg;
        NetPackLen(buf, "%b%b%s", msgno, p->statusno, p->statustext.bs);
    } else if (msgno == REGISTERMSG) {
        RegisterMsg *p = msg;
        NetPackLen(buf, "%b%s%s", msgno, p->alias.bs, p->pwd.bs);
    } else if (msgno == LOGINMSG) {
        LoginMsg *p = msg;
        NetPackLen(buf, "%b%s%s", msgno, p->alias.bs, p->pwd.bs);
    } else if (msgno == COMMANDMSG) {
        CommandMsg *p = msg;
        NetPackLen(buf, "%b%s", msgno, p->command.bs);
    } else if (msgno == ALIASESMSG) {
        AliasesMsg *p = msg;
        NetPackLen(buf, "%b%s", msgno, p->aliases.bs);
    } else {
        fprintf(stderr, "pack_message(): Unrecognized msgno %d\n", msgno);
    }
}

void MsgUnpack(Msg *msg, char *msgbytes, u16 len) {
    u8 msgno = MSGNO(msgbytes);
    if (msgno == STATUSMSG) {
        StatusMsg *p = (StatusMsg *) msg;
        p->statustext = StringNew("");
        NetUnpack(msgbytes, len, "%b%b%s", &p->msgno, &p->statusno, &p->statustext);
    } else if (msgno == REGISTERMSG) {
        RegisterMsg *p = (RegisterMsg *) msg;
        p->alias = StringNew("");
        p->pwd = StringNew("");
        NetUnpack(msgbytes, len, "%b%s%s", &p->msgno, &p->alias, &p->pwd);
    } else if (msgno == LOGINMSG) {
        LoginMsg *p = (LoginMsg *) msg;
        p->alias = StringNew("");
        p->pwd = StringNew("");
        NetUnpack(msgbytes, len, "%b%s%s", &p->msgno, &p->alias, &p->pwd);
    } else if (msgno == COMMANDMSG) {
        CommandMsg *p = (CommandMsg *) msg;
        p->command = StringNew("");
        NetUnpack(msgbytes, len, "%b%s", &p->msgno, &p->command);
    } else if (msgno == ALIASESMSG) {
        AliasesMsg *p = (AliasesMsg *) msg;
        p->aliases = StringNew("");
        NetUnpack(msgbytes, len, "%b%s", &p->msgno, &p->aliases);
    } else {
        msg->msgno = 0;
        fprintf(stderr, "unpack_message(): Unrecognized msgno %d\n", msgno);
    }
}

