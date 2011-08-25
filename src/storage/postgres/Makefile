CC      = gcc
CFLAGS  = -Wall -I/usr/include/libxml2 -pthread
LIBS    = -ldl  -lxml2 -lcommlbr -lpq
INCLUDE = "-I../../../"

all: postgres_output.so

postgres_output.so: postgres_output.o
	$(CC) $(CFLAGS) $(LIBS) -shared -Wl,-soname,postgres_output.so.1 postgres_output.o -o $@ -lc
	
postgres_output.o: postgres_output.c
	$(CC) $(CFLAGS) $(INCLUDE) -fPIC -c -o $@ $<

clean:
	-rm postgres_output.so postgres_output.o

