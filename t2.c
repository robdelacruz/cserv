#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clib.h"
#include "cnet.h"
#include "data.h"

void printmap(Map m) {
    printf("map len: %d cap: %d\n", m.len, m.cap);
    for (int i=0; i < m.len; i+=2) {
        printf("  %s: '%s'\n", (char *) m.items[i], (char *) m.items[i+1]);
    }
}

void maptest() {
    Map m = MapNew(1);
    MapSet(&m, "abc", "Here's a string");
    MapSet(&m, "def", "def string");
    MapSet(&m, "ghi", "ghi string");
    MapSet(&m, "abc", "abc string");
    MapSet(&m, "def", "DEF string");
    printmap(m);

    MapClear(&m);
    printmap(m);
    MapFree(m);
}

void printuser(User u) {
    printf("user '%.*s' '%.*s'\n", u.alias.len, u.alias.bs, u.pwdhash.len, u.pwdhash.bs);
}
void validate_pwd(User u, char *pwd) {
    if (UserValidatePwd(u, pwd))
        printf("Valid password: %.*s - '%s'\n", u.alias.len, u.alias.bs, pwd);
    else
        printf("Invalid password: %.*s - '%s'\n", u.alias.len, u.alias.bs, pwd);
}
void testuser() {
    User u1 = UserNew("rob", "password123");
    printuser(u1);

    validate_pwd(u1, "password123");
    validate_pwd(u1, "password1234");
    validate_pwd(u1, "password12");
    validate_pwd(u1, "password");
    validate_pwd(u1, "abc");
    validate_pwd(u1, " ");
    validate_pwd(u1, "");

}

void register_user(ServerData *sd, char *alias, char *pwd) {
    User u = UserNew(alias, "");
    UserSetPassword(&u, pwd);
    UsersAppend(&sd->users, u);
}

int main(int argc, char *argv[]) {
    ServerData sd = ServerDataNew();
    register_user(&sd, "rob", "password123");
    register_user(&sd, "robtwister", "twister123");
    register_user(&sd, "abcuser", "abcpassword");

    for (int i=0; i < sd.users.len; i++) {
        User u = sd.users.items[i];
        printuser(u);
    }

    ServerDataSave(sd);

    ServerDataFree(sd);
    return 0;
}
