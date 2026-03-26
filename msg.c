#include <stdio.h>
#include <stdlib.h>

#include "clib.h"
#include "msg.h"

void print_message(void *msg) {
    u8 msgid = GET_MSGID(msg);

    if (msgid == MSGID_IDENTITY) {
        IdentityMsg *p = msg;
        printf("** Identity [%d] alias: '%.*s' **\n", p->seq, p->alias.len, p->alias.bs);
    } else if (msgid == MSGID_ACK) {
        AckMsg *p = msg;
        printf("** Ack [%d] text: '%.*s' **\n", p->seq, p->acktext.len, p->acktext.bs);
    } else if (msgid == MSGID_COMMAND) {
        CommandMsg *p = msg;
        printf("** Command [%d] text: '%.*s' **\n", p->seq, p->command.len, p->command.bs);
    } else if (msgid == MSGID_CHAT) {
        ChatMsg *p = msg;
        printf("** Chat [%d] from: '%.*s' to: '%.*s' text: '%.*s' **\n", p->seq, p->from_alias.len, p->from_alias.bs, p->to_alias.len, p->to_alias.bs, p->text.len, p->text.bs);
    }
}

void *unpack_message(char *msgbytes, u16 len) {
    u8 msgid = GET_MSGID(msgbytes);

    if (msgid == MSGID_IDENTITY) {
        IdentityMsg *p = (IdentityMsg *) malloc(sizeof(IdentityMsg));
        p->alias = StringNew("");
        NetUnpack(msgbytes, len, "%b%w%s", &p->msgid, &p->seq, &p->alias);
        return p;
    } else if (msgid == MSGID_ACK) {
        AckMsg *p = (AckMsg *) malloc(sizeof(AckMsg));
        p->acktext = StringNew("");
        NetUnpack(msgbytes, len, "%b%w%s", &p->msgid, &p->seq, &p->acktext);
        return p;
    } else if (msgid == MSGID_COMMAND) {
        CommandMsg *p = (CommandMsg *) malloc(sizeof(CommandMsg));
        p->command = StringNew("");
        NetUnpack(msgbytes, len, "%b%w%s", &p->msgid, &p->seq, &p->command);
        return p;
    } else if (msgid == MSGID_CHAT) {
        ChatMsg *p = (ChatMsg *) malloc(sizeof(ChatMsg));
        p->from_alias = StringNew("");
        p->to_alias = StringNew("");
        p->text = StringNew("");
        NetUnpack(msgbytes, len, "%b%w%s%s%s", &p->msgid, &p->seq, &p->from_alias, &p->to_alias, &p->text);
        return p;
    }

    fprintf(stderr, "unpack_message(): Unrecognized msgid %d\n", msgid);
    return NULL;
}

void free_message(void *msg) {
    u8 msgid = GET_MSGID(msg);

    if (msgid == MSGID_IDENTITY) {
        IdentityMsg *p = (IdentityMsg *) msg;
        StringFree(&p->alias);
        free(p);
    } else if (msgid == MSGID_ACK) {
        AckMsg *p = (AckMsg *) msg;
        StringFree(&p->acktext);
        free(p);
    } else if (msgid == MSGID_COMMAND) {
        CommandMsg *p = (CommandMsg *) msg;
        StringFree(&p->command);
        free(p);
    } else if (msgid == MSGID_CHAT) {
        ChatMsg *p = (ChatMsg *) msg;
        StringFree(&p->from_alias);
        StringFree(&p->to_alias);
        StringFree(&p->text);
        free(p);
    } else {
        fprintf(stderr, "free_message(): Unrecognized msgid %d\n", msgid);
    }
}


