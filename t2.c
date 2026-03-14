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

short twoscomp(unsigned short u16) {
    short i16;

    if (u16 <= 0x7fff)
        return u16;

    i16 = -1 - (0xffff-u16);
    return i16;

}
void twoscomp_test() {
    unsigned short u16s[] = {1, 2, 0, 32700, 32767, 32768, 65535};

    for (int i=0; i < countof(u16s); i++) {
        unsigned short us = u16s[i];
        short s = twoscomp(us);

        printf("unsigned short: %d = short %d\n", us, s);
    }

    short n = -2;
    printf("twoscomp(%d) = %d\n", (unsigned short) n, twoscomp((unsigned short) n));
}

// pack(buf, "BUF %b %w %l %s", n1, n2, n3, "abc");
void pack(Buffer *buf, char *fmt, ...) {
    int state = 0;  // 0: none, 1: prev '%'
    va_list args;

    va_start(args, fmt);
    for (char *pfmt = fmt; *pfmt != 0; pfmt++) {
        if (state == 0) {
            if (*pfmt == '%') {
                state = 1;
                continue;
            }
            BufferAppendChar(buf, *pfmt);
            continue;
        }
        if (state == 1) {
            if (*pfmt == 'b') {
                u8 ch = va_arg(args, int);
                BufferAppendChar(buf, ch);
            } else if (*pfmt == 'w') {
                u16 w = va_arg(args, int);
                BufferAppendChar(buf, w >> 8);
                BufferAppendChar(buf, w);
            } else if (*pfmt == 'l') {
                u32 l = va_arg(args, u32);
                BufferAppendChar(buf, l >> 24);
                BufferAppendChar(buf, l >> 16);
                BufferAppendChar(buf, l >> 8);
                BufferAppendChar(buf, l);
            } else if (*pfmt == 's') {
                char *s = va_arg(args, char *);
                u16 slen = strlen(s);
                BufferAppendChar(buf, slen >> 8);
                BufferAppendChar(buf, slen);
                BufferAppend(buf, s, slen);
            } else {
                // Ignore any unsupported %? spec
            }
            state = 0;
            continue;
        }
    }
    va_end(args);
}

#define INCREMENT_BLK(p, n, endp) { \
    p += n;                         \
    if (p >= endp) break;           \
}
// unpack(buf, "BUF %b %w %l %s", &n1, &n2, &n3, s1);
void unpack(char *blk, int blklen, char *fmt, ...) {
    int state = 0;  // 0: none, 1: prev '%'
    va_list args;

    va_start(args, fmt);
    char *endblk = blk+blklen;
    for (char *pfmt = fmt; *pfmt != 0; pfmt++) {
        assert(blk < endblk);

        if (state == 0) {
            if (*pfmt == '%') {
                state = 1;
                continue;
            }
            INCREMENT_BLK(blk, 1, endblk);
            continue;
        }
        if (state == 1) {
            if (*pfmt == 'b') {
                u8 *ch = va_arg(args, u8*);
                *ch = *blk;
                INCREMENT_BLK(blk, 1, endblk);
            } else if (*pfmt == 'w') {
                u16 *w = va_arg(args, u16*);
                *w = *blk << 8;
                INCREMENT_BLK(blk, 1, endblk);
                *w |= *blk;
                INCREMENT_BLK(blk, 1, endblk);
            } else if (*pfmt == 'l') {
                u32 *l = va_arg(args, u32 *);
                *l = *blk << 24;
                INCREMENT_BLK(blk, 1, endblk);
                *l |= *blk << 16;
                INCREMENT_BLK(blk, 1, endblk);
                *l |= *blk << 8;
                INCREMENT_BLK(blk, 1, endblk);
                *l |= *blk;
                INCREMENT_BLK(blk, 1, endblk);
            } else if (*pfmt == 's') {
                String *str = (String *) va_arg(args, void *);
                u16 slen = *blk << 8;
                INCREMENT_BLK(blk, 1, endblk);
                slen |= *blk;
                INCREMENT_BLK(blk, 1, endblk);
                StringAssignFromBytes(str, blk, slen);
                INCREMENT_BLK(blk, slen, endblk);
            } else {
                // Ignore any unsupported %? spec
            }
            state = 0;
            continue;
        }
    }
    va_end(args);
}

int main(int argc, char *argv[]) {
    Buffer buf = BufferNew(10);

    pack(&buf, "%w%b%w   %s%b%s%w", 0x1234, 0x9d, 0xfdea, "abc123def", 0xd8, "1234 567", 0x7770);

    for (int i=0; i < buf.len; i++) {
        printf("%.2x ", (unsigned char) buf.bs[i]);
    }
    printf("\n");

    u16 w1, w2, w3;
    u8 b1, b2;
    String str1 = StringNew("");
    String str2 = StringNew("");
    unpack(buf.bs, buf.len, "%w%b%w   %s%b%s%w", &w1, &b1, &w2, &str1, &b2, &str2, &w3);
    printf("w1: %x w2: %x w3: %x\n", w1, w2, w3);
    printf("b1: %x b2: %x\n", b1, b2);
    printf("str1: '%.*s'\n", str1.len, str1.bs);
    printf("str2: '%.*s'\n", str2.len, str2.bs);

    BufferFree(&buf);

    return 0;
}
