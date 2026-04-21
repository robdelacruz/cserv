#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>

#include "clib.h"

void panic(char *s) {
    fprintf(stderr, "%s\n", s);
    abort();
}

void *malloc0(size_t size) {
    void *p = malloc(size);
    memset(p, 0, size);
    return p;
}
void *realloc0(void *ptr, size_t oldsize, size_t newsize) {
    assert(newsize > oldsize);
    void *p = realloc(ptr, newsize);
    memset(p + oldsize, 0, newsize - oldsize);
    return p;
}

Arena ArenaNew(u32 cap) {
    Arena a;
    if (cap == 0)
        cap = 1024;
    a.bs = malloc0(cap);
    a.pos = 0;
    a.cap = cap;
    return a;
}
void ArenaFree(Arena *a) {
    free(a->bs);
    memset(a, 0, sizeof(Arena));
}
void ArenaReset(Arena *a) {
    a->pos = 0;
}
void *ArenaAlloc(Arena *a, u32 size) {
    if (a->pos + size > a->cap) {
        fprintf(stderr, "ArenaAlloc() size: %ld not enough memory\n", size);
        abort();
    }
    u8 *p = a->bs + a->pos;
    a->pos += size;
    return p;
}
void *ArenaPushBytes(Arena *a, void *src, u32 size) {
    if (a->pos + size > a->cap) {
        fprintf(stderr, "ArenaAlloc() size: %ld not enough memory\n", size);
        abort();
    }
    u8 *p = a->bs + a->pos;
    memcpy(p, src, size);
    a->pos += size;
    return p;
}
void ArenaGet(Arena *a, void *dest, u32 offset, u32 size) {
    if (offset+size > a->pos) {
        fprintf(stderr, "ArenaGet() offset: %ld size: %ld pos: %ld out of bounds\n", offset, size, a->pos);
        memset(dest, 0, size);
        return;
    }
    memcpy(dest, a->bs+offset, size);
}

String StringNew0() {
    String str;
    str.len = 0;
    str.bs = (char *) malloc0(1);
    str.bs[0] = 0;
    return str;
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
    memset(str, 0, sizeof(String));
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
void StringAssignFromBytes(String *str, char *bs, int bslen) {
    StringFree(str);
    *str = StringNewFromBytes(bs, bslen);
}
void StringAssignFormat(String *str, const char *fmt, ...) {
    StringFree(str);
    va_list args;

    va_start(args, fmt);
    str->len = vsnprintf(NULL, 0, fmt, args);
    str->bs = (char *) malloc(str->len+1);
    va_end(args);

    va_start(args, fmt);
    vsnprintf(str->bs, str->len+1, fmt, args);
    va_end(args);
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
int StringEquals(String str, char *s) {
    int slen = strlen(s);
    if (str.len != slen)
        return 0;
    for (int i=0; i < str.len; i++) {
        if (str.bs[i] != s[i])
            return 0;
    }
    return 1;
}
void StringTrim(String str) {
    if (str.len == 0)
        return;

    // set starti to index of first non-whitespace char
    // set endi to index of last non-whitespace char
    int starti=0;
    int endi=str.len-1;
    for (int i=0; i < str.len; i++) {
        if (!isspace(str.bs[i]))
            break;
        starti++;
    }
    for (int i=str.len-1; i >= 0; i--) {
        if (!isspace(str.bs[i]))
            break;
        endi--;
    }
    if (starti > str.len-1) {
        memset(str.bs, 0, str.len);
        str.len = 0;
        return;
    }
    assert(endi >= starti);

    int newlen = endi-starti+1;
    memmove(str.bs, str.bs+starti, newlen);
    memset(str.bs+newlen, 0, str.len-newlen);
    str.len = newlen;
}

// Returns array of tokens.
// Returned array should be freed with ArrayFree().
Array StringSplit(String str, char *sep) {
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

    Array toks = ArrayNew(ntoks, sizeof(String), (FreeFunc) StringFree);
    itokstart = 0;
    while (1) {
        int isep = StringSearch(str, itokstart, sep);
        if (isep == -1)
            toklen = str.len - itokstart;
        else
            toklen = isep - itokstart;

        String tok = StringNewFromBytes(str.bs+itokstart, toklen);
        ArrayAppend(&toks, &tok);

        if (isep == -1)
            break;
        itokstart = isep + sep_len;
    }

    return toks;
}

Buffer BufferNew(u32 cap) {
    Buffer buf;
    if (cap == 0)
        cap = 1024;
    buf.bs = (char *) malloc0(cap);
    buf.len = 0;
    buf.cap = cap;
    return buf;
}
void BufferFree(Buffer *buf) {
    free(buf->bs);
    memset(buf, 0, sizeof(Buffer));
}
void BufferClear(Buffer *buf) {
    memset(buf->bs, 0, buf->len);
    buf->len = 0;
}
void BufferAppend(Buffer *buf, char *bs, u32 bslen) {
    assert(buf->len <= buf->cap);

    // If more space needed, keep doubling capacity until there's enough space.
    u32 newcap = buf->cap;
    while (bslen > newcap - buf->len)
        newcap *= 2;
    if (newcap > buf->cap) {
        buf->bs = (char *) realloc0(buf->bs, buf->cap, newcap);
        buf->cap = newcap;
    }
    assert(bslen <= buf->cap - buf->len);

    memcpy(buf->bs + buf->len, bs, bslen);
    buf->len += bslen;
}
void BufferAppendChar(Buffer *buf, char c) {
    BufferAppend(buf, &c, 1);
}
// Remove first n bytes of buffer
void BufferShift(Buffer *buf, int n) {
    assert(buf->len <= buf->cap);
    if (n > buf->len)
        n = buf->len;

    memcpy(buf->bs,
           buf->bs + n,
           buf->len - n);
    buf->len -= n;
    memset(buf->bs + buf->len, 0, buf->cap - buf->len);
}

Map MapNew(u16 cap) {
    Map m;
    if (cap == 0)
        cap = 32;
    cap*=2;
    m.items = (void **) malloc0(sizeof(void *)*cap);
    m.len = 0;
    m.cap = cap;
    m.isfreevals = 0;
    return m;
}
void MapFree(Map *m) {
    for (int i=0; i < m->len; i+=2) {
        free(m->items[i]);
        if (m->isfreevals)
            free(m->items[i+1]);
    }
    free(m->items);
    memset(m, 0, sizeof(Map));
}
void MapClear(Map *m) {
    for (int i=0; i < m->len; i+=2) {
        free(m->items[i]);
        if (m->isfreevals)
            free(m->items[i+1]);
    }
    memset(m->items, 0, sizeof(void *)*m->len);
    m->len = 0;
}
void MapSet(Map *m, char *k, void *v) {
    assert(m->len <= m->cap);

    // Overwrite v if k already in map.
    for (int i=0; i < m->len; i+=2) {
        if (strcmp(m->items[i], k) == 0) {
            if (m->isfreevals)
                free(m->items[i+1]);
            m->items[i+1] = v;
            return;
        }
    }

    // Double the capacity if more space needed.
    if (m->len == m->cap) {
        m->items = (void **) realloc0(m->items, sizeof(void *)*m->cap, sizeof(void *)*m->cap * 2);
        m->cap *= 2;
    }
    assert(m->len < m->cap);

    m->items[m->len] = strdup(k);
    m->items[m->len+1] = v;
    m->len += 2;
}
void *MapGet(Map m, char *k) {
    for (int i=0; i < m.len; i+=2) {
        if (strcmp(m.items[i], k) == 0)
            return m.items[i+1];
    }
    return NULL;
}
void MapRemove(Map *m, char *k) {
    for (int i=0; i < m->len; i+=2) {
        if (strcmp(m->items[i], k) == 0) {
            free(m->items[i]);
            if (m->isfreevals) {
                free(m->items[i+1]);
            }
            // Move last item to the spot where the deleted item is.
            m->items[i] = m->items[m->len-2];
            m->items[i+1] = m->items[m->len-1];

            memset(&m->items[m->len-2], 0, sizeof(void *)*2);
            m->len -= 2;
        }
    }
}

DBMap DBMapNew(u16 cap, void (*freeval)(KVItem)) {
    DBMap m;
    if (cap == 0)
        cap = 32;
    m.items = malloc0(sizeof(KVItem)*cap);
    m.len = 0;
    m.cap = cap;
    m.freeval = freeval;
    return m;
}
void DBMapFree(DBMap *m) {
    for (int i=0; i < m->len; i++)
        m->freeval(m->items[i]);
    free(m->items);
    memset(m, 0, sizeof(DBMap));
}
void DBMapClear(DBMap *m) {
    for (int i=0; i < m->len; i++)
        m->freeval(m->items[i]);
    memset(m->items, 0, sizeof(KVItem)*m->len);
    m->len = 0;
}
void DBMapSet(DBMap *m, char *k, DBVar v) {
    for (int i=0; i < m->len; i++) {
        KVItem item = m->items[i];
        if (strcmp(item.key, k) == 0) {
            m->freeval(item);
            strncpy(item.key, k, sizeof(item.key));
            item.key[sizeof(item.key)-1] = 0;
            item.val = v;
            return;
        }
    }

    assert(m->len <= m->cap);
    if (m->len == m->cap) {
        m->items = realloc0(m->items, sizeof(KVItem)*m->cap, sizeof(KVItem)*m->cap * 2);
        m->cap *= 2;
    }
    assert(m->len < m->cap);

    KVItem item;
    strncpy(item.key, k, sizeof(item.key));
    item.key[sizeof(item.key)-1] = 0;
    item.val = v;
    m->items[m->len] = item;
    m->len++;
}
DBVar *DBMapGet(DBMap m, char *k) {
    for (int i=0; i < m.len; i++) {
        KVItem item = m.items[i];
        if (strcmp(item.key, k) == 0)
            return &m.items[i].val;
    }
    return NULL;
}
void DBMapRemove(DBMap *m, char *k) {
    for (int i=0; i < m->len; i++) {
        KVItem item = m->items[i];
        if (strcmp(item.key, k) == 0) {
            m->freeval(item);
            // Move last item to the spot where the deleted item is.
            if (m->len > 1)
                m->items[i] = m->items[m->len-1];
            memset(&m->items[m->len-1], 0, sizeof(KVItem));
            m->len--;
            return;
        }
    }
}

static void _freeitem(void *p) {
}
static void *array_item_ptr(Array *a, int index) {
    return a->items + index*a->itemsize;
}

Array ArrayNew(u16 cap, int itemsize, FreeFunc freeitem) {
    Array a;
    if (cap == 0)
        cap = 32;
    if (freeitem == NULL)
        freeitem = _freeitem;

    a.items = malloc0(itemsize*cap);
    a.itemsize = itemsize;
    a.len = 0;
    a.cap = cap;
    a.freeitem = freeitem;
    return a;
}
void ArrayFree(Array *a) {
    for (int i=0; i < a->len; i++)
        a->freeitem(array_item_ptr(a, i));
    free(a->items);
    memset(a, 0, sizeof(Array));
}
void ArrayClear(Array *a) {
    memset(a->items, 0, a->itemsize*a->len);
    a->len = 0;
}
void ArrayAppend(Array *a, void *item) {
    assert(a->len <= a->cap);

    // Double the capacity if more space needed
    if (a->len == a->cap) {
        a->items = realloc0(a->items, a->itemsize*a->cap, a->itemsize*a->cap*2);
        a->cap *= 2;
    }
    assert(a->len < a->cap);

    memcpy(array_item_ptr(a, a->len), item, a->itemsize);
    a->len++;
}
void ArrayRemove(Array *a, int index) {
    if (index < 0  || index >= a->len)
        return;

    a->freeitem(a->items + index*a->itemsize);
    if (a->len > 1)
        memmove(array_item_ptr(a, index), array_item_ptr(a, index+1), (a->len - index) * a->itemsize);
    memset(array_item_ptr(a, a->len-1), 0, a->itemsize);
    a->len--;
}
void *ArrayItem(Array a, int index) {
    assert(index >= 0 && index < a.len);
    return array_item_ptr(&a, index);
}


