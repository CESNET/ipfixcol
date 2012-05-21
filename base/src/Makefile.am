SUBDIRS = . input/tcp input/udp input/ipfix storage/ipfix storage/dummy ipfixviewer

if HAVE_SCTP
SUBDIRS += input/sctp
endif

bin_PROGRAMS = ipfixcol
AM_CPPFLAGS = -I$(top_srcdir)/headers
AM_CFLAGS = -rdynamic

ipfixcol_SOURCES = \
	config.c \
	config.h \
	data_manager.c \
	data_manager.h \
	ipfixcol.c \
	preprocessor.c \
	preprocessor.h \
	queues.c \
	queues.h \
	template_manager.c \
	verbose.c