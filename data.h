#ifndef DATA_H
#define DATA_H

#include <stdlib.h>
#include <stdio.h>
#include "clib.h"

#define SERVER_OK 0
#define ERR_USER_ALIAS_EXISTS 1

typedef struct {
    String alias;
    String pwdhash;
} User;

typedef struct {
    User *items;
    u16 len;
    u16 cap;
    i8 do_free;
} Users;

typedef struct {
    Users users;
} ServerData;

User UserNew(char *alias, char *pwdhash);
void UserSetPassword(User *u, char *pwd);
void UserFree(User u);
int UserValidatePwd(User u, char *pwd);

Users UsersNew(u32 cap);
void UsersFree(Users a);
void UsersClear(Users *a);
void UsersAppend(Users *a, User o);
void UsersRemove(Users *a, User o);
User* UsersFind(Users a, char *alias);

char* server_strerror(int z);

ServerData ServerDataNew();
void ServerDataFree(ServerData sd);
void ServerDataLoad(ServerData *sd);
void ServerDataSave(ServerData sd);
int RegisterUser(ServerData *sd, char *alias, char *pwd);

#endif
