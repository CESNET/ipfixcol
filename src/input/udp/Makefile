################################################
# Makefile for the ipfixcol input plugin test  #
################################################

CC      = gcc
CFLAGS  = -Wall

all: udp_input.so

# udp_input.so
udp_input.so: udp_input.o
	$(CC) $(CFLAGS) -shared -Wl,-soname,udp_input.so.1 -lcommlbr -lxml2 -o $@ $< -lc
	rm udp_input.o
	
udp_input.o: udp_input.c
	$(CC) $(CFLAGS) -I../../../ -I/usr/include/libxml2 -fPIC -c $<

clean:
	rm -f udp_input.so

