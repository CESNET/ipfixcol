bin_PROGRAMS = fbitdump
AM_CPPFLAGS = @AM_CPPFLAGS@ -I$(top_srcdir)/3rdparty
AM_CXXFLAGS = @AM_CXXFLAGS@ -fno-strict-aliasing
LDADD = ../3rdparty/libpugixml.a
fbitdump_SOURCES = \
	Column.cpp \
	Column.h \
	Configuration.cpp \
	Configuration.h \
	Cursor.cpp \
	Cursor.h \
	DefaultOutput.cpp \
	DefaultOutput.h \
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
	scanner.cpp \
	scanner.h


MAINTAINERCLEANFILES = scanner.h scanner.cpp protocols.cpp
EXTRA_DIST = scanner.h scanner.cpp scanner.l protocols.cpp
# Such scripts for which a build rule has been supplied need to be deleted explicitly using CLEANFILES (see Clean), and their sources have to be distributed, usually with EXTRA_DIST (see Basics of Distribution).

fbitdump.$(OBJEXT): scanner.h

scanner.h scanner.cpp: scanner.l
	$(FLEX) -o scanner.cpp --header-file=scanner.h $<

protocols.cpp: /etc/protocols
	awk -f $(top_srcdir)/tools/generate_protocols.awk /etc/protocols > protocols.cpp
