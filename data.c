#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <crypt.h>

#include "clib.h"
#include "data.h"

#define CRYPTSALT "salt1234567890"

String password_hash(String phrase) {
    if (phrase.len > CRYPT_MAX_PASSPHRASE_SIZE)
        phrase.bs[CRYPT_MAX_PASSPHRASE_SIZE] = 0;

    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    char *pz = crypt_r(phrase.bs, CRYPTSALT, &data);
    assert(pz != NULL);

    return StringNew(data.output);
}
int password_verify(String phrase, String hash) {
    if (phrase.len > CRYPT_MAX_PASSPHRASE_SIZE)
        phrase.bs[CRYPT_MAX_PASSPHRASE_SIZE] = 0;

    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    char *pz = crypt_r(phrase.bs, CRYPTSALT, &data);
    assert(pz != NULL);

    return StringEquals(hash, data.output);
}

User UserNew(char *alias, char *pwdhash) {
    User u;
    u.alias = StringNew(alias);
    u.pwdhash = StringNew(pwdhash);
    return u;
}
void UserSetPassword(User *u, char *pwd) {
    String spassword = StringNew(pwd);
    String hash = password_hash(spassword);
    StringAssign(&u->pwdhash, hash.bs);

    StringFree(spassword);
    StringFree(hash);
}
void UserFree(User u) {
    StringFree(u.alias);
    StringFree(u.pwdhash);
}
int UserValidatePwd(User u, char *pwd) {
    String spassword = StringNew(pwd);
    String hash = password_hash(spassword);
    StringFree(spassword);

    return StringEquals(u.pwdhash, hash.bs);
}

Users UsersNew(u32 cap) {
    Users a;
    if (cap == 0)
        cap = 32;
    a.items = malloc(sizeof(User)*cap);
    memset(a.items, 0, sizeof(User)*cap);
    a.len = 0;
    a.cap = cap;
    a.do_free = 0;
    return a;
}
void UsersFree(Users a) {
    if (a.do_free) {
        for (int i=0; i < a.len; i++)
            UserFree(a.items[i]);
    }
    free(a.items);
}
void UsersClear(Users *a) {
    if (a->do_free) {
        for (int i=0; i < a->len; i++)
            UserFree(a->items[i]);
    }
    memset(a->items, 0, sizeof(User)*a->len);
    a->len = 0;
}
void UsersAppend(Users *a, User o) {
    assert(a->len <= a->cap);
    // Double the capacity if more space needed.
    if (a->len == a->cap) {
        a->items = realloc(a->items, sizeof(o)*a->cap * 2);
        memset(a->items + sizeof(o)*a->cap, 0, sizeof(o)*a->cap);
        a->cap *= 2;
    }
    assert(a->len < a->cap);
    a->items[a->len] = o;
    a->len++;
}
void UsersRemove(Users *a, User o) {
    for (int i=0; i < a->len; i++) {
        if (StringEquals(a->items[i].alias, o.alias.bs)) {
            if (a->do_free)
                UserFree(a->items[i]);
            // Move last index to the delete index.
            a->items[i] = a->items[a->len-1];
            memset(&a->items[a->len-1], 0, sizeof(o));
            a->len--;
            return;
        }
    }
}
User *UsersFind(Users a, char *alias) {
    for (int i=0; i < a.len; i++) {
        if (StringEquals(a.items[i].alias, alias))
            return &a.items[i];
    }
    return NULL;
}

char* server_strerror(int z) {
    if (z == SERVER_OK)
        return "OK";
    else if (z == ERR_USER_ALIAS_EXISTS)
        return "User alias exists";
    else
        return "Unknown error";
}

ServerData ServerDataNew() {
    ServerData sd;
    sd.users = UsersNew(32);
    return sd;
}
void ServerDataFree(ServerData sd) {
    UsersFree(sd.users);
}

#define USERSFILE "users.txt"

void ServerDataLoad(ServerData *sd) {
    char line[1024];
    String sline = StringNew("");

    UsersClear(&sd->users);

    FILE *f = fopen(USERSFILE, "r");
    if (f == NULL) {
        fprintf(stderr, "Can't open file '%s' for read: %s\n", USERSFILE, strerror(errno));
        return;
    }
    while(1) {
        if (fgets(line, sizeof(line), f) == NULL)
            break;
        StringAssign(&sline, line);
        StringTrim(sline);
        StringList ss = StringSplit(sline, ";");
        ss.isfreeitems = 1;

        if (ss.len >= 2) {
            User u = UserNew(ss.items[0].bs, ss.items[1].bs);
            UsersAppend(&sd->users, u);
        }
        StringListFree(ss);
    }
    fclose(f);
}
void ServerDataSave(ServerData sd) {
    FILE *f = fopen(USERSFILE, "w");
    if (f == NULL) {
        fprintf(stderr, "Can't open file '%s' for write: %s\n", USERSFILE, strerror(errno));
        return;
    }
    for (int i=0; i < sd.users.len; i++) {
        User u = sd.users.items[i];
        fprintf(f, "%s;%s\n", u.alias.bs, u.pwdhash.bs);
    }

    fclose(f);
}

int RegisterUser(ServerData *sd, char *alias, char *pwd) {
    if (UsersFind(sd->users, alias) != NULL)
        return ERR_USER_ALIAS_EXISTS;

    User u = UserNew(alias, "");
    UserSetPassword(&u, pwd);
    UsersAppend(&sd->users, u);
    return SERVER_OK;
}

