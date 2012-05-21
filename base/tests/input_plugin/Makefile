################################################
# Makefile for the ipfixcol input plugin test  #
################################################

CC      = gcc
CFLAGS  = -Wall -rdynamic
LIBS    = -ldl
INCLUDE = "-I../../headers"

SOURCES = input_test.c ../../src/verbose.c

all: input_test

# input_test
input_test: $(SOURCES)
	$(CC) $(CFLAGS) $(LIBS) $(INCLUDE) -o $@ $(SOURCES)

clean:
	rm -f input_test

