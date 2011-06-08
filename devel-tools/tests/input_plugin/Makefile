################################################
# Makefile for the ipfixcol input plugin test  #
################################################

CC      = gcc
CFLAGS  = -Wall -ldl

all: input_test

# input_test
input_test: input_test.c
	$(CC) $(CFLAGS) -I../../../ -o $@ $<

clean:
	rm -f input_test

