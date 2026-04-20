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

void freeval(KVItem item) {
}

void testdbvar() {
    DBVar v;
    DBMap m = DBMapNew(0, freeval);
    DBMapClear(&m);

    v.n = 123;
    DBMapSet(&m, "amt", v);

    v.f = 1000.23;
    DBMapSet(&m, "price", v);

    v.s = "password123";
    DBMapSet(&m, "password", v);

    DBVar *amt, *price, *password, *none;
    amt = DBMapGet(m, "amt");
    price = DBMapGet(m, "price");
    password = DBMapGet(m, "password");
    none = DBMapGet(m, "abc");

    assert(amt != NULL);
    assert(price != NULL);
    assert(password != NULL);
    assert(none == NULL);

    printf("amt: %d price: %.2f password: '%s'\n", amt->n, price->f, password->s);

    DBMapFree(m);
}

void print_hostctxs(Array a) {
    for (int i=0; i < a.len; i++) {
        HostCtx *hc = ArrayItem(a, i);
        printf("[%d] '%s': %d\n", i, hc->alias.bs, hc->fd);
    }
}

int main(int argc, char *argv[]) {
    Array hostctxs = ArrayNew(0, sizeof(HostCtx), (FreeFunc) HostCtxFree);
    ArrayClear(&hostctxs);

    HostCtx hc = HostCtxNew(10);
    hc.alias = StringNew("host 10");
    ArrayAppend(&hostctxs, &hc);
    hc = HostCtxNew(11);
    hc.alias = StringNew("host 11");
    ArrayAppend(&hostctxs, &hc);
    hc = HostCtxNew(12);
    hc.alias = StringNew("host 12");
    ArrayAppend(&hostctxs, &hc);
    print_hostctxs(hostctxs);
    printf("\n");

    ArrayRemove(&hostctxs, 4);
    ArrayRemove(&hostctxs, -1);
    ArrayRemove(&hostctxs, 10);

    ArrayRemove(&hostctxs, 0);
    ArrayRemove(&hostctxs, 1);
    print_hostctxs(hostctxs);

    ArrayFree(hostctxs);
    return 0;
}
