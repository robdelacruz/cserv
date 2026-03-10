#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clib.h"

int main(int argc, char *argv[]) {
    Buffer buf = BufferNew(10);
    BufferAppend(&buf, "abc", 3);
    BufferAppend(&buf, "def", 3);
    BufferAppend(&buf, "ghi", 3);
    printf("buf: '%.*s' len: %ld\n", buf.len, buf.bs, buf.len);
    buf.cur = 9;
    BufferResetFromCur(&buf);
    printf("buf: '%.*s' len: %ld\n", buf.len, buf.bs, buf.len);
    return 0;
}

