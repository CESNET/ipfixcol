AM_CPPFLAGS += -I$(top_srcdir)/headers

noinst_LIBRARIES = libprofiles.a

libprofiles_a_CFLAGS = -fPIC
libprofiles_a_SOURCES = \
	Channel.cpp Channel.h \
	profiles.cpp profiles_internal.h \
	Profile.cpp Profile.h \
	filter.c filter.h \
	parser.c parser.h \
	scanner.c scanner.h

CLEANFILES = parser.h parser.c scanner.h scanner.c
BUILT_SOURCES = parser.h parser.c scanner.h scanner.c

parser.h parser.c: parser.y
	$(BISON) --defines=parser.h -o parser.c $<

scanner.h scanner.c: scanner.l
	$(FLEX) -o scanner.c --header-file=scanner.h $<

EXTRA_DIST = parser.y scanner.l
