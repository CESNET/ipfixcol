SUBDIRS = plugins

bin_PROGRAMS = fbitdump

AM_CPPFLAGS = @AM_CPPFLAGS@ -I$(top_srcdir)/3rdparty
AM_CXXFLAGS = @AM_CXXFLAGS@ -fno-strict-aliasing
LDADD = ../3rdparty/libpugixml.a

fbitdump_SOURCES = \
	AggregateFilter.cpp \
	AggregateFilter.h \
	Column.cpp \
	Column.h \
	Configuration.cpp \
	Configuration.h \
	Cursor.cpp \
	Cursor.h \
	DefaultPlugin.cpp \
	DefaultPlugin.h \
	fbitdump.cpp \
	Filter.cpp \
	Filter.h \
	IndexManager.cpp \
	IndexManager.h \
	Printer.cpp \
	Printer.h \
	protocols.h \
	protocols.cpp \
	Resolver.cpp \
	Resolver.h \
	Table.cpp \
	Table.h \
	TableManager.cpp \
	TableManagerCursor.cpp \
	TableManagerCursor.h \
	TableManager.h \
	TableSummary.cpp \
	TableSummary.h \
	TemplateInfo.cpp \
	TemplateInfo.h \
	typedefs.h \
	Utils.cpp \
	Utils.h \
	Values.cpp \
	Values.h \
	Verbose.cpp \
	Verbose.h \
	scanner.cpp \
	scanner.h \
	parser.h \
	parser.cpp \
	stack.hh \
	location.hh \
	position.hh

# Build these before everyting else
BUILT_SOURCES = parser.h parser.cpp stack.hh location.hh position.hh scanner.h scanner.cpp protocols.cpp

MAINTAINERCLEANFILES = parser.h parser.cpp stack.hh location.hh position.hh scanner.h scanner.cpp protocols.cpp
EXTRA_DIST = parser.h parser.cpp stack.hh location.hh position.hh parser.y scanner.h scanner.cpp scanner.l protocols.cpp
# Such scripts for which a build rule has been supplied need to be deleted explicitly using CLEANFILES (see Clean), and their sources have to be distributed, usually with EXTRA_DIST (see Basics of Distribution).

parser.h parser.cpp stack.hh location.hh position.hh: parser.y
	$(BISON) --defines=parser.h -o parser.cpp $<

scanner.h scanner.cpp: scanner.l
	$(FLEX) -o scanner.cpp --header-file=scanner.h $<

protocols.cpp: /etc/protocols
	awk -f $(top_srcdir)/tools/generate_protocols.awk /etc/protocols > protocols.cpp
