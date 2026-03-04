LIBS=
CFLAGS=-std=gnu99 -Wall -Werror
CFLAGS+= -Wno-deprecated-declarations -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable

SOURCE=t.c clib.c

all: t

t: $(SOURCE)
	gcc -o $@ $^ $(LIBS)

clean:
	rm -rf t

