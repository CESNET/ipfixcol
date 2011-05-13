##########################################
# Makefile for the ipfixcol bitmap tools #
##########################################

CC      = gcc
CFLAGS  = -Wall -g #-O3 
#flags for debugging: -g -pg -Wextra

all: bmgen bmread compaxgen compaxread ozrgen ozrread

# bmgen
bmgen: bmgen.c
	$(CC) $(CFLAGS) -o $@ $<

compaxgen: compaxgen.c
	$(CC) $(CFLAGS) -o $@ $<

compaxread: compaxread.c
	$(CC) $(CFLAGS) -o $@ $<

ozrgen: ozrgen.c
	$(CC) $(CFLAGS) -o $@ $<

ozrread: ozrread.c
	$(CC) $(CFLAGS) -o $@ $<

# bmread
bmread: bmread.c
	$(CC) $(CFLAGS) -o $@ $<


clean:
	rm -f bmgen bmread compaxgen compaxread ozrgen ozrread

