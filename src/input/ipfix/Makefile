CC      = gcc
CFLAGS  = -Wall -Wextra

all: ipfix_input.so

ipfix_input.so: ipfix_input.o
	$(CC) $(CFLAGS) -shared -Wl,-soname,ipfix_input.so.1 -lcommlbr ipfix_input.o -o $@ -lc
	
ipfix_input.o: ipfix_file.c
	$(CC) $(CFLAGS) -I../../../ -fPIC -c -o $@ ipfix_file.c

clean:
	rm -f ipfix_input.so ipfix_input.o

