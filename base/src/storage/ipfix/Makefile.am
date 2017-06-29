pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-ipfix-output.la
ipfixcol_ipfix_output_la_LDFLAGS = -module -avoid-version -shared

ipfixcol_ipfix_output_la_SOURCES = \
    ipfix_file.c ipfix_file.h \
    configuration.c configuration.h \
    odid.c odid.h \
    files.c files.h

if HAVE_DOC
MANSRC = ipfixcol-ipfix-output.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-ipfix-output.1
CLEANFILES = ipfixcol-ipfix-output.1
endif

%.1 : %.dbk
	@if [ -n "$(XSLTPROC)" ]; then \
		if [ -f "$(XSLTMANSTYLE)" ]; then \
			echo $(XSLTPROC) $(XSLTMANSTYLE) $<; \
			$(XSLTPROC) $(XSLTMANSTYLE) $<; \
		else \
			echo "Missing $(XSLTMANSTYLE)!"; \
			exit 1; \
		fi \
	else \
		echo "Missing xsltproc"; \
	fi

