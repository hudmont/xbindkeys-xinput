CC=gcc
LIBS=xi x11 guile-2.0 popt libffi
DEFINES=-DPACKAGE_VERSION=\"1.9beta\" -DAVOID_KNOWN_HARMLESS_WARNINGS -DDEBUG
CFLAGS=-g -Wall -Wextra `pkg-config --cflags $(LIBS)` $(DEFINES)

SRCS=$(wildcard *.c)
OBJS = $(SRCS:.c=.o)

LDFLAGS=`pkg-config --libs $(LIBS)`

xbindkeys: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJS) xbindkeys

.PHONY: lint
lint:
	clang-tidy $(SRCS)  $(DEFINES)
.PHONY: format
format:
	clang-format *.c *.h
