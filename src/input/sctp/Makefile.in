CC=gcc
CFLAGS=-Wall -I/usr/include/libxml2 -pthread
LIBS=-ldl -lxml2 -lcommlbr -lsctp
DEBUG=-g
INCLUDE="-I../../../"

all: sctp_input.so

sctp_input.so: sctp_input.o
	$(CC) $(CFLAGS) $(LIBS) -shared -Wl,-soname,sctp_input.so.1 \
	sctp_input.o -o $@ -lc
	
sctp_input.o: sctp_input.c
	$(CC) $(DEBUG) $(CFLAGS) $(INCLUDE) -fPIC -c -o $@ $<

clean:
	-rm sctp_input.so sctp_input.o

