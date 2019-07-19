CC=gcc
LIBS=xi x11 guile-2.0 popt libffi
DEFINES=-DPACKAGE_VERSION=\"1.9beta\" -DAVOID_KNOWN_HARMLESS_WARNINGS -DDEBUG -DFORK_FLAG
CFLAGS=-g -std=gnu11 -Wall -Wextra `pkg-config --cflags $(LIBS)` $(DEFINES)

SRCS=$(wildcard *.c)
HDRS=$(wildcard *.h)
OBJS = $(SRCS:.c=.o)

LDFLAGS=`pkg-config --libs $(LIBS)`

xbindkeys: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJS) xbindkeys

.PHONY: lint
lint:
	clang-tidy $(SRCS) $(HDRS) -- $(CFLAGS)
