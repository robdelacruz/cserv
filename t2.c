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
    MapFree(&m);
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

    DBMapFree(&m);
}

#define HOSTCTXS(v) ((HostCtx *) v)

void print_hostctxs(Array a) {
    //HostCtx *hcs = CAST(a.items, HostCtx*);
    HostCtx *hcs = HOSTCTXS(a.items);
    for (int i=0; i < a.len; i++) {
        printf("[%d] '%s': %d\n", i, hcs[i].username.bs, hcs[i].fd);
    }
}

void testarray() {
    Array hostctxs = ArrayNew(0, sizeof(HostCtx), (FreeFunc) HostCtxFree);
    ArrayClear(&hostctxs);

    HostCtx hc = HostCtxNew(10);
    hc.username = StringNew("host 10");
    ArrayAppend(&hostctxs, &hc);
    hc = HostCtxNew(11);
    hc.username = StringNew("host 11");
    ArrayAppend(&hostctxs, &hc);
    hc = HostCtxNew(12);
    hc.username = StringNew("host 12");
    ArrayAppend(&hostctxs, &hc);
    print_hostctxs(hostctxs);
    printf("\n");

    ArrayRemove(&hostctxs, 4);
    ArrayRemove(&hostctxs, -1);
    ArrayRemove(&hostctxs, 10);

    ArrayRemove(&hostctxs, 0);
    ArrayRemove(&hostctxs, 1);
    print_hostctxs(hostctxs);

    ArrayFree(&hostctxs);


}

int main(int argc, char *argv[]) {
    String s = StringNew("abc; def; ghi;  ");
    Array toks = StringSplit(s, "; ");

    String *ss = (String *) toks.items;
    for (int i=0; i < toks.len; i++)
        printf("[%d] '%s'\n", i, ss[i].bs);
    printf("\n");

    ArrayFree(&toks);

    return 0;
}
