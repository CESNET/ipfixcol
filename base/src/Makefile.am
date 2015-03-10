SUBDIRS = utils \
	. \
	input/tcp input/udp input/ipfix \
	intermediate/anonymization intermediate/dummy intermediate/joinflows \
	intermediate/filter intermediate/odip intermediate/hooks \
	storage/ipfix storage/dummy storage/forwarding \
	ipfixviewer

if HAVE_SCTP
SUBDIRS += input/sctp
endif

bin_PROGRAMS = ipfixcol
AM_CPPFLAGS += -I$(top_srcdir)/headers
AM_CFLAGS += -rdynamic

# create config.h with proper paths to configuration files
EXTRA_DIST = config.h.in
CLEANFILES = config.h

# Build these before everyting else
BUILT_SOURCES = config.h

# Rebuild after executing configure
config.h: config.h.in ../config.status
	sed 's|@sysconfdir[@]|$(sysconfdir)|g' $< > $@

ipfixcol_LDADD = utils/profiles/libprofiles.a
ipfixcol_LDFLAGS = -lstdc++

ipfixcol_SOURCES = \
	config.c \
	config.h \
	configurator.c \
	configurator.h \
	crc.c \
	crc.h \
	data_manager.c \
	data_manager.h \
	intermediate_process.c \
	intermediate_process.h \
	ipfix_message.c \
	ipfixcol.c \
	output_manager.c \
	output_manager.h \
	preprocessor.c \
	preprocessor.h \
	queues.c \
	queues.h \
	template_manager.c \
	verbose.c \
	utils/utils.c
