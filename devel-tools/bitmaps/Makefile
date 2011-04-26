##########################################
# Makefile for the ipfixcol bitmap tools #
##########################################

CC      = gcc
CFLAGS  = -Wall -O3
#flags for debugging: -g -pg -Wextra

all: bmgen bmread

# bmgen
bmgen: bmgen.c
	$(CC) $(CFLAGS) -o $@ $<

# bmread
bmread: bmread.c
	$(CC) $(CFLAGS) -o $@ $<


clean:
	rm -f bmgen bmread

