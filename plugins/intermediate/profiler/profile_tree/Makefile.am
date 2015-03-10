AM_CPPFLAGS += -I$(top_srcdir)/headers

noinst_LTLIBRARIES = libprofiletree.la

libprofiletree_la_CFLAGS = -fPIC
libprofiletree_la_SOURCES = \
	Channel.cpp Channel.h \
	profile_tree.cpp profile_tree.h \
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
