AM_CPPFLAGS += \
	-fPIC \
	-I$(top_srcdir)/headers \
	-I$(top_srcdir)/src/utils/filter
	-I$(top_srcdir)/src #last one is for config.h in ffilter

# C code must be able to propagate C++ exceptions
# Note: This can happen in callbacks of profile events
AM_CFLAGS += -fPIC -fexceptions

noinst_LIBRARIES = libprofiles.a

libprofiles_a_SOURCES = \
	Channel.cpp Channel.h \
	profiles.cpp profiles_internal.h \
	Profile.cpp Profile.h \
	bitset.c bitset.h \
	profile_events.c

