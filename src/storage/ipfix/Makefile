CC      = gcc
CFLAGS  = -Wall -Wextra

all: ipfix_output.so

ipfix_output.so: ipfix_output.o
	$(CC) $(CFLAGS) -shared -Wl,-soname,ipfix_output.so.1 -lcommlbr ipfix_output.o -o $@ -lc
	
ipfix_output.o: ipfix_file.c
	$(CC) $(CFLAGS) -I../../../ -fPIC -c -o $@ ipfix_file.c

clean:
	rm -f ipfix_output.so ipfix_output.o

