#ifndef CLIB_H
#define CLIB_H

#define countof(v) (sizeof(v) / sizeof((v)[0]))
#define memzero(p, v) (memset(p, 0, sizeof(v)))
#define CAST(v, type) ((type) (v))

typedef char i8;
typedef short i16;
typedef long i32;
typedef long long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

typedef struct {
    u8 *bs;
    u32 pos;
    u32 cap;
} Arena;

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
    u32 len;
    u32 cap;
} Buffer;

typedef struct {
    void **items;
    u16 len;
    u16 cap;
    i8 isfreevals;
} Map;

typedef union _DBVar {
    i32 n32;
    i64 n64;
    int n;
    double f;
    char *s;
} DBVar;

typedef struct {
    char key[32];
    DBVar val;
} KVItem;

typedef struct {
    KVItem *items;
    u16 len;
    u16 cap;
    void (*freeval)(KVItem);
} DBMap;

typedef void (*FreeFunc)(void *);

typedef struct {
    void *items;
    int itemsize;
    u16 len;
    u16 cap;
    FreeFunc freeitem;
} Array;

void panic(char *s);

Arena ArenaNew(u32 cap);
void ArenaFree(Arena a);
void ArenaReset(Arena *a);
void *ArenaAlloc(Arena *a, u32 size);
void *ArenaPushBytes(Arena *a, void *src, u32 size);
void ArenaGet(Arena *a, void *dest, u32 offset, u32 size);

String StringNew0();
String StringNew(char *s);
String StringNewFromBytes(char *bs, int bslen);
void StringFree(String str);
String StringDup(String src);
String StringFormat(const char *fmt, ...);
void StringAppend(String *str, char *s);
void StringAssign(String *str, char *s);
void StringAssignFromBytes(String *str, char *bs, int bslen);
void StringAssignFormat(String *str, const char *fmt, ...);
int StringSearch(String str, int startpos, char *searchstr);
int StringEquals(String str, char *s);
StringList StringSplit(String str, char *sep);
void StringTrim(String str);

StringList StringListNew(u16 cap);
void StringListFree(StringList sl);
void StringListAppend(StringList *sl, String str);

Buffer BufferNew(u32 cap);
void BufferFree(Buffer buf);
void BufferClear(Buffer *buf);
void BufferAppend(Buffer *buf, char *bs, u32 bslen);
void BufferAppendChar(Buffer *buf, char c);
void BufferShift(Buffer *buf, int n);

Map MapNew(u16 cap);
void MapFree(Map m);
void MapClear(Map *m);
void MapSet(Map *m, char *k, void *v);
void *MapGet(Map m, char *k);
void MapRemove(Map *m, char *k);

DBMap DBMapNew(u16 cap, void (*freeval)(KVItem));
void DBMapFree(DBMap m);
void DBMapClear(DBMap *m);
void DBMapSet(DBMap *m, char *k, DBVar v);
DBVar *DBMapGet(DBMap m, char *k);
void DBMapRemove(DBMap *m, char *k);

Array ArrayNew(u16 cap, int itemsize, FreeFunc freeitem);
void ArrayFree(Array a);
void ArrayClear(Array *a);
void ArrayAppend(Array *a, void *item);
void ArrayRemove(Array *a, int index);
void *ArrayItem(Array a, int index);

#endif
