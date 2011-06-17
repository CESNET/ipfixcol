CC=gcc
CFLAGS=-Wall -Wextra `pkg-config --cflags libxml-2.0`
LIBS=`pkg-config --libs libxml-2.0`

all: ipfix_output.so

ipfix_output.so: ipfix_output.o
	$(CC) $(CFLAGS) $(LIBS) -shared -Wl,-soname,ipfix_output.so.1 -lcommlbr ipfix_output.o -o $@ -lc
	
ipfix_output.o: ipfix_file.c
	$(CC) $(CFLAGS) -I../../../ -fPIC -c -o $@ ipfix_file.c

clean:
	-rm ipfix_output.so ipfix_output.o

