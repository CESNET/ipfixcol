pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-filter-inter.la
ipfixcol_filter_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_filter_inter_la_LIBADD = -lrt

ipfixcol_filter_inter_la_SOURCES = filter.c parser.c scanner.c filter.h parser.h scanner.h

CLEANFILES = parser.h parser.c scanner.h scanner.c

BUILT_SOURCES = parser.h parser.c scanner.h scanner.c 

parser.h parser.c: parser.y
	$(BISON) --defines=parser.h -o parser.c $<

scanner.h scanner.c: scanner.l
	$(FLEX) -o scanner.c --header-file=scanner.h $<
