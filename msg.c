#include <stdio.h>
#include <stdlib.h>

#include "clib.h"
#include "msg.h"

void print_message(void *msg) {
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

void *unpack_message(char *msgbytes, u16 len) {
    u8 msgno = MSGNO(msgbytes);

    if (msgno == STATUSMSG) {
        StatusMsg *p = (StatusMsg *) malloc(sizeof(StatusMsg));
        p->statustext = StringNew("");
        NetUnpack(msgbytes, len, "%b%b%s", &p->msgno, &p->statusno, &p->statustext);
        return p;
    } else if (msgno == REGISTERMSG) {
        RegisterMsg *p = (RegisterMsg *) malloc(sizeof(RegisterMsg));
        p->alias = StringNew("");
        p->pwd = StringNew("");
        NetUnpack(msgbytes, len, "%b%s%s", &p->msgno, &p->alias, &p->pwd);
        return p;
    } else if (msgno == LOGINMSG) {
        LoginMsg *p = (LoginMsg *) malloc(sizeof(LoginMsg));
        p->alias = StringNew("");
        p->pwd = StringNew("");
        NetUnpack(msgbytes, len, "%b%s%s", &p->msgno, &p->alias, &p->pwd);
        return p;
    } else if (msgno == COMMANDMSG) {
        CommandMsg *p = (CommandMsg *) malloc(sizeof(CommandMsg));
        p->command = StringNew("");
        NetUnpack(msgbytes, len, "%b%s", &p->msgno, &p->command);
        return p;
    } else if (msgno == ALIASESMSG) {
        AliasesMsg *p = (AliasesMsg *) malloc(sizeof(AliasesMsg));
        p->aliases = StringNew("");
        NetUnpack(msgbytes, len, "%b%s", &p->msgno, &p->aliases);
        return p;
    }

    fprintf(stderr, "unpack_message(): Unrecognized msgno %d\n", msgno);
    return NULL;
}

void free_message(void *msg) {
    u8 msgno = MSGNO(msg);

    if (msgno == STATUSMSG) {
        StatusMsg *p = msg;
        StringFree(p->statustext);
        free(p);
    } else if (msgno == REGISTERMSG) {
        RegisterMsg *p = msg;
        StringFree(p->alias);
        StringFree(p->pwd);
        free(p);
    } else if (msgno == LOGINMSG) {
        LoginMsg *p = msg;
        StringFree(p->alias);
        StringFree(p->pwd);
        free(p);
    } else if (msgno == COMMANDMSG) {
        CommandMsg *p = msg;
        StringFree(p->command);
        free(p);
    } else if (msgno == ALIASESMSG) {
        AliasesMsg *p = msg;
        StringFree(p->aliases);
        free(p);
    } else {
        fprintf(stderr, "free_message(): Unrecognized msgno %d\n", msgno);
    }
}


