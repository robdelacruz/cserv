LIBS=-lcrypt
CFLAGS=-std=gnu99 -Wall -Werror
CFLAGS+= -Wno-deprecated-declarations -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable

SOURCE=t.c clib.c cnet.c data.c msg.c 

all: t

t: $(SOURCE)
	gcc -o $@ $^ $(LIBS)

t2: t2.c clib.c cnet.c data.c
	gcc -o $@ $^ $(LIBS)

tclient: tclient.c msg.c clib.c cnet.c data.c
	gcc -o $@ $^ $(LIBS)

clean:
	rm -rf t t2 tclient

