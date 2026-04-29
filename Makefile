LIBS=-lcrypt
CFLAGS=-std=gnu99 -Wall -Werror
CFLAGS+= -Wno-deprecated-declarations -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
GTK_CFLAGS=`pkg-config --cflags gtk+-3.0`
GTK_LIBS=`pkg-config --libs gtk+-3.0`

SOURCE=t.c db.c clib.c cnet.c sqlite3.o

all: t

sqlite3.o: sqlite3.c
	gcc -c -o $@ $^

t: $(SOURCE)
	gcc $(CFLAGS) -o $@ $^ $(LIBS)

t2: t2.c clib.c cnet.c
	gcc -o $@ $^ $(LIBS)

tclient: tclient.c clib.c cnet.c
	gcc -o $@ $^ $(LIBS)

tm: tm.c clib.c cnet.c uistuff.c
	gcc $(CFLAGS) $(GTK_CFLAGS) -o $@ $^ $(GTK_LIBS)

clean:
	rm -rf t t2 tclient tm

