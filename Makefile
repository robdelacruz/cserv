LIBS=
CFLAGS=-std=gnu99 -Wall -Werror
CFLAGS+= -Wno-deprecated-declarations -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable

SOURCE=t.c clib.c cnet.c

all: t

t: $(SOURCE)
	gcc -o $@ $^ $(LIBS)

t2: t2.c clib.c cnet.c
	gcc -o $@ $^

tclient: tclient.c clib.c cnet.c
	gcc -o $@ $^

clean:
	rm -rf t t2 tclient

