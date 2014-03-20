SUBDIRS = . \
	input/tcp input/udp input/ipfix \
	intermediate/anonymization intermediate/dummy \
	storage/ipfix storage/dummy \
	ipfixviewer

if HAVE_SCTP
SUBDIRS += input/sctp
endif

bin_PROGRAMS = ipfixcol
AM_CPPFLAGS = -I$(top_srcdir)/headers
AM_CFLAGS = -rdynamic -g

ipfixcol_SOURCES = \
	config.c \
	config.h \
	crc.c \
	crc.h \
	data_manager.c \
	data_manager.h \
	intermediate_process.c \
	intermediate_process.h \
	ipfix_message.c \
	ipfix_message.h \
	ipfixcol.c \
	output_manager.c \
	output_manager.h \
	preprocessor.c \
	preprocessor.h \
	queues.c \
	queues.h \
	template_manager.c \
	verbose.c
