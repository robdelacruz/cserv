#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "clib.h"

void panic(char *s) {
    fprintf(stderr, "%s\n", s);
    abort();
}

String StringNew(char *s) {
    String str;
    str.len = strlen(s);
    str.bs = (char *) malloc(str.len+1);
    memcpy(str.bs, s, str.len);    
    str.bs[str.len] = 0;
    return str;
}
String StringNewFromBytes(char *bs, int bslen) {
    String str;
    str.bs = (char *) malloc(bslen+1);
    memcpy(str.bs, bs, bslen);
    str.bs[bslen] = 0;
    str.len = bslen;
    return str;
}
void StringFree(String *str) {
    free(str->bs);
    str->bs = 0;
    str->len = 0;
}
String StringDup(String src) {
    String str;
    str.len = src.len;
    str.bs = (char *) malloc(str.len+1);
    memcpy(str.bs, src.bs, str.len);    
    str.bs[str.len] = 0;
    return str;
}
String StringFormat(const char *fmt, ...) {
    String str;
    va_list args;

    va_start(args, fmt);
    str.len = vsnprintf(NULL, 0, fmt, args);
    str.bs = (char *) malloc(str.len+1);
    va_end(args);

    va_start(args, fmt);
    vsnprintf(str.bs, str.len+1, fmt, args);
    va_end(args);

    return str;
}
void StringAppend(String *str, char *s) {
    int slen = strlen(s);
    if (str->len + slen + 1 > USHRT_MAX) // Check for str.len overflow
        return;

    str->bs = (char *) realloc(str->bs, str->len+slen+1);
    memcpy(str->bs + str->len, s, slen);
    str->len += slen;
    str->bs[str->len] = 0;
}
void StringAssign(String *str, char *s) {
    StringFree(str);
    *str = StringNew(s);
}
int StringSearch(String str, int startpos, char *searchstr) {
    int searchstr_len = strlen(searchstr);
    for (int i=startpos; i < str.len; i++) {
        for (int isearch=0, istr=i; isearch < searchstr_len && istr < str.len; isearch++, istr++) {
            if (str.bs[istr] != searchstr[isearch])
                break;
            if (isearch == searchstr_len-1) // Match found
                return i;
        }
    }
    return -1;
}

StringList StringListNew(u16 cap) {
    StringList sl;
    if (cap == 0)
        cap = 32;
    sl.items = (String *) malloc(sizeof(String)*cap);
    memset(sl.items, 0, sizeof(String)*cap);
    sl.len = 0;
    sl.cap = cap;
    sl.isfreeitems = 0;
    return sl;
}
void StringListFree(StringList *sl) {
    if (sl->isfreeitems) {
        for (int i=0; i < sl->len; i++)
            StringFree(&sl->items[i]);
    }
    free(sl->items);
    sl->items = 0;
    sl->len = 0;
}
void StringListAppend(StringList *sl, String str) {
    assert(sl->len <= sl->cap);

    // Double the capacity if more space needed.
    if (sl->len == sl->cap) {
        sl->items = (String *) realloc(sl->items, sizeof(String)*sl->cap * 2);
        memset(sl->items + sizeof(String)*sl->cap, 0, sizeof(String)*sl->cap);
        sl->cap *= 2;
    }
    assert(sl->len < sl->cap);

    sl->items[sl->len] = str;
    sl->len++;
}
// Returns tokens as stringlist.
// Each string in returned stringlist should be freed with StringFree()
StringList StringSplit(String str, char *sep) {
    int itokstart=0;
    int toklen=0;
    int sep_len = strlen(sep);

    // ntoks = number of tokens after splitting string
    int ntoks=1;
    while (1) {
        int isep = StringSearch(str, itokstart, sep);
        if (isep == -1)
            break;

        ntoks++;
        itokstart = isep + sep_len;
    }

    StringList sl = StringListNew(ntoks);
    sl.isfreeitems = 1;
    itokstart = 0;
    while (1) {
        int isep = StringSearch(str, itokstart, sep);
        if (isep == -1)
            toklen = str.len - itokstart;
        else
            toklen = isep - itokstart;

        String tok = StringNewFromBytes(str.bs+itokstart, toklen);
        StringListAppend(&sl, tok);

        if (isep == -1)
            break;
        itokstart = isep + sep_len;
    }

    return sl;
}

Buffer BufferNew(u32 cap) {
    Buffer buf;
    if (cap == 0)
        cap = 1024;
    buf.bs = (char *) malloc(cap);
    memset(buf.bs, 0, cap);
    buf.cur = 0;
    buf.len = 0;
    buf.cap = cap;
    return buf;
}
void BufferFree(Buffer *buf) {
    free(buf->bs);
    buf->bs = 0;
    buf->cur = 0;
    buf->len = 0;
    buf->cap = 0;
}
void BufferClear(Buffer *buf) {
    memset(buf->bs, 0, buf->len);
    buf->cur = 0;
    buf->len = 0;
}
void BufferAppend(Buffer *buf, char *bs, u32 bslen) {
    assert(buf->len <= buf->cap);

    // If more space needed, keep doubling capacity until there's enough space.
    u32 newcap = buf->cap;
    while (bslen > newcap - buf->len)
        newcap *= 2;
    if (newcap > buf->cap) {
        buf->bs = (char *) realloc(buf->bs, newcap);
        memset(buf->bs + buf->cap, 0, newcap - buf->cap);
        buf->cap = newcap;
        printf("newcap: %ld\n", buf->cap);
    }
    assert(bslen <= buf->cap - buf->len);

    memcpy(buf->bs + buf->len, bs, bslen);
    buf->len += bslen;
}


