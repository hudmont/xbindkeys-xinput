CC=gcc
#LIBS=xi x11 guile-2.0 popt
CFLAGS=-g -Wall -Wextra `pkg-config --cflags xi x11 guile-2.0 popt` -DPACKAGE_VERSION=\"2.0beta\"
#CXXFLAGS=$CFLAGS
SRCS=$(wildcard *.c)
OBJS = $(SRCS:.c=.o)
#DEPS = $(OBJS:.o=.d)  # one dependency file for each source

# $(LDFLAGS)
LDFLAGS=`pkg-config --libs xi x11 guile-2.0 popt`

#-include $(DEPS)   # include all dep files in the makefile

# rule to generate a dep file by using the C preprocessor
# (see man cpp for details on the -MM and -MT options)
#%.d: %.c
#	@$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@


#PROG = xbindkeys

#all: $(PROG)
xbindkeys: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
#xbindkeys:
#	cc $(SRCS) -o $@ $^ $(LDFLAGS) 
#$(LDFLAGS)
.PHONY: clean
clean:
	rm -f $(OBJS) xbindkeys

#.PHONY: cleandep
#cleandep:
#	rm -f $(DEPS)

