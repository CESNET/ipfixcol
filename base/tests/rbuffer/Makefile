CC=gcc -std=gnu99 -Wall
CFLAGS=-I../../headers -g
LIBS= -pthread
OBJ = queues.o rbuffer_test.o verbose.o

rbuffer_test: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)
	rm -f $(OBJ)

queues.o: ../../src/queues.c
	$(CC) $(CFLAGS) -c -o $@ $<

verbose.o: ../../src/verbose.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
	
clean:
	rm -f $(OBJ) rbuffer_test