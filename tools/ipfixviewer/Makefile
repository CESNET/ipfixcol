CC=gcc
CFLAGS=-Wall -Wextra -I../../headers/
LDFLAGS=
DEBUG=-g

all: ipfixviewer

ipfixviewer: ipfix-viewer.c templates.c
	$(CC) $(CFLAGS) $(DEBUG) ipfix-viewer.c templates.c -o ipfixviewer

clean:
	-rm ipfixviewer

