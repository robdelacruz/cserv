#ifndef CLIB_H
#define CLIB_H

#define countof(v) (sizeof(v) / sizeof((v)[0]))
#define memzero(p, v) (memset(p, 0, sizeof(v)))

typedef char i8;
typedef short i16;
typedef long i32;
typedef long long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

typedef struct {
    char *bs;
    u16 len;
} String;

typedef struct {
    String *items;
    u16 len;
    u16 cap;
    i8 isfreeitems;
} StringList;

typedef struct {
    char *bs;
    u32 cur;
    u32 len;
    u32 cap;
} Buffer;

void panic(char *s);

String StringNew(char *s);
String StringNewFromBytes(char *bs, int bslen);
void StringFree(String *str);
String StringDup(String src);
String StringFormat(const char *fmt, ...);
void StringAppend(String *str, char *s);
void StringAssign(String *str, char *s);
int StringSearch(String str, int startpos, char *searchstr);
StringList StringSplit(String str, char *sep);

StringList StringListNew(u16 cap);
void StringListFree(StringList *sl);
void StringListAppend(StringList *sl, String str);

Buffer BufferNew(u32 cap);
void BufferFree(Buffer *buf);
void BufferClear(Buffer *buf);
void BufferAppend(Buffer *buf, char *bs, u32 bslen);
void BufferResetFromCur(Buffer *buf);

#endif
