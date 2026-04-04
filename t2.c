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

int main(int argc, char *argv[]) {
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

    return 0;
}
