CC=gcc
CFLAGS=-Wall -Wextra `pkg-config --cflags libxml-2.0`
LIBS=`pkg-config --libs libxml-2.0` -lcommlbr

all: ipfix_input.so

ipfix_input.so: ipfix_input.o
	$(CC) $(CFLAGS) $(LIBS) -shared -Wl,-soname,ipfix_input.so.1 ipfix_input.o -o $@ -lc
	
ipfix_input.o: ipfix_file.c
	$(CC) $(CFLAGS) -I../../../ -fPIC -c -o $@ ipfix_file.c

clean:
	-rm ipfix_input.so ipfix_input.o

