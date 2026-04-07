LIBS=-lcrypt
CFLAGS=-std=gnu99 -Wall -Werror
CFLAGS+= -Wno-deprecated-declarations -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
GTK_CFLAGS=`pkg-config --cflags gtk+-3.0`
GTK_LIBS=`pkg-config --libs gtk+-3.0`

SOURCE=t.c clib.c cnet.c data.c msg.c 

all: t

t: $(SOURCE)
	gcc $(CFLAGS) -o $@ $^ $(LIBS)

t2: t2.c clib.c cnet.c data.c
	gcc -o $@ $^ $(LIBS)

tclient: tclient.c msg.c clib.c cnet.c data.c
	gcc -o $@ $^ $(LIBS)

tm: tm.c msg.c clib.c cnet.c
	gcc $(CFLAGS) $(GTK_CFLAGS) -o $@ $^ $(GTK_LIBS)

clean:
	rm -rf t t2 tclient tm

