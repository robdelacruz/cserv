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

void splittest() {
    String s = StringNew("abc; def; ghi;  ");
    Array toks = StringSplit(s, "; ");

    String *ss = (String *) toks.items;
    for (int i=0; i < toks.len; i++)
        printf("[%d] '%s'\n", i, ss[i].bs);
    printf("\n");

    ArrayFree(&toks);
}

void string0test() {
    char bytes[] = "abcdefg";
    String s1 = {0};
    String s2 = {0};
    String s3 = {0};

    StringFree(&s1);
    printf("s1 a: '%.*s'\n", s1.len, s1.bs);
    printf("s1 b: '%s'\n", CSTR(s1));

    StringAppend(&s2, "abc");
    printf("s2 a: '%.*s'\n", s2.len, s2.bs);
    printf("s2 b: '%s'\n", CSTR(s2));

    s2 = (String) {0};
    printf("s2 a: '%.*s'\n", s2.len, s2.bs);
    printf("s2 b: '%s'\n", CSTR(s2));

    s2 = StringNew0();
    printf("s2 a: '%.*s'\n", s2.len, s2.bs);
    printf("s2 b: '%s'\n", CSTR(s2));
    StringAssign(&s2, "abc");
    printf("s2 b: '%s'\n", CSTR(s2));

    s1 = (String) {0};
    s2 = StringDup(s1);
    printf("s2 a: '%.*s'\n", s2.len, s2.bs);
    printf("s2 b: '%s'\n", CSTR(s2));
    StringAssign(&s1, "abc");
    s2 = StringDup(s1);
    StringAssign(&s1, "def");
    printf("s2 a: '%.*s'\n", s2.len, s2.bs);
    printf("s2 b: '%s'\n", CSTR(s2));

    s2 = (String) {0};
    StringAssignFromBytes(&s2, bytes, sizeof(bytes));
    printf("s2 a: '%.*s'\n", s2.len, s2.bs);

    s2 = (String) {0};
    StringAssignFormat(&s2, "Hello bytes '%s'\n", bytes);
    printf("s2 a: '%.*s'\n", s2.len, s2.bs);

    s2 = (String) {0};
    int ifound = StringSearch(s2, 0, "abc");
    printf("ifound: %d\n", ifound);
    ifound = StringSearch(s2, 1, "abc");
    printf("ifound: %d\n", ifound);
    StringAssign(&s2, "ABCabcDEFdef");
    ifound = StringSearch(s2, 0, "abc");
    printf("ifound: %d\n", ifound);
    ifound = StringSearch(s2, 2, "abc");
    printf("ifound: %d\n", ifound);
    ifound = StringSearch(s2, 3, "abc");
    printf("ifound: %d\n", ifound);
    ifound = StringSearch(s2, 4, "abc");
    printf("ifound: %d\n", ifound);

    s2 = (String) {0};
    int iequals = StringEquals(s2, "a");
    printf("iequals: %d\n", iequals);
    iequals = StringEquals(s2, "");
    printf("iequals: %d\n", iequals);
    s1 = (String) {0};
    iequals = StringEquals(s2, CSTR(s1));
    printf("iequals: %d\n", iequals);
    StringAssign(&s1, "abc");
    iequals = StringEquals(s2, CSTR(s1));
    printf("iequals: %d\n", iequals);

    s2 = (String) {0};
    Array ss = StringSplit(s2, ";");
    printf("ss.len: %d\n", ss.len);
    String *ssitems = (String *) ss.items;
    for (int i=0; i < ss.len; i++)
        printf("ss[%d]: '%s'\n", i, CSTR(ssitems[i]));

    s2 = (String) {0};
    StringTrim(s2);
    printf("s2 a: '%.*s'\n", s2.len, s2.bs);
    s2 = StringNew(" ");
    StringTrim(s2);
    printf("s2 b: '%.*s'\n", s2.len, s2.bs);
    s2 = StringNew(" abc   ");
    StringTrim(s2);
    printf("s2 b: '%.*s'\n", s2.len, s2.bs);
}


int main(int argc, char *argv[]) {

    return 0;
}
