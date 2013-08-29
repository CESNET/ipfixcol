bin_PROGRAMS = expiredaemon
AM_CPPFLAGS = @AM_CPPFLAGS@
AM_CFLAGS = @AM_CFLAGS@ -fno-strict-aliasing -D_GNU_SOURCE
expiredaemon_LDFLAGS = -pthread
expiredaemon_SOURCES = \
	expire.c \
	expire.h \
	buffer.c \
	buffer.h \
	delete_queue.c \
	delete_queue.h \
	scan.c \
	scan.h \
	watcher.c \
	watcher.h \
	pipe.c \
	pipe.h
