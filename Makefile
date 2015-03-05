all: node
.PHONY : all

CLIBS=-pthread
CC=gcc
CPPFLAGS=
CFLAGS=-g

NODEOBJS=node.o tools.o 

$(NODEOBJS) : %.o: %.c tools.h msg.h
	$(CC) -c $<

node: $(NODEOBJS)
	$(CC) -o node $(NODEOBJS)  $(CLIBS)



clean:
	rm -f *.o
	rm -f node

