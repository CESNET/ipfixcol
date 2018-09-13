SUBDIRS = utils \
	. \
	input/tcp input/udp input/ipfix \
	intermediate/anonymization intermediate/dummy intermediate/joinflows \
	intermediate/filter intermediate/odip intermediate/hooks intermediate/timenow \
	intermediate/timecheck storage/ipfix storage/dummy storage/forwarding \
	ipfixviewer

if HAVE_SCTP
SUBDIRS += input/sctp
endif

bin_PROGRAMS = \
	ipfixcol \
	ipfixcol-profiles-check \
	ipfixcol-filter-check

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

# This is a command for the linker to include all symbols (unused for plugins too)
# There MUST NOT be any whitespace around commas!
ipfixcol_LDFLAGS = \
	-Wl,--whole-archive,utils/elements/libelements.a,utils/profiles/libprofiles.a,utils/template_mapper/libtmapper.a,--no-whole-archive

ipfixcol_LDADD = \
	utils/filter/libfilter.a \
	-lstdc++

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

# Profiles validator
ipfixcol_profiles_check_LDADD = \
	utils/profiles/libprofiles.a \
	utils/filter/libfilter.a \
	utils/elements/libelements.a \
	-lstdc++

ipfixcol_profiles_check_SOURCES = \
	profiles_check.c \
	utils/utils.c \
	ipfix_message.c \
	template_manager.c \
	verbose.c

# Filter validator
ipfixcol_filter_check_LDADD = \
	utils/filter/libfilter.a \
	utils/elements/libelements.a \
	-lstdc++

ipfixcol_filter_check_SOURCES = \
	filter_check.c \
	utils/utils.c \
	ipfix_message.c \
	template_manager.c \
	verbose.c
