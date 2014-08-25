pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-ipfix-input.la
ipfixcol_ipfix_input_la_LDFLAGS = -module -avoid-version -shared

ipfixcol_ipfix_input_la_SOURCES = ipfix_file.c

if HAVE_DOC
MANSRC = ipfixcol-ipfix-input.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-ipfix-input.1
CLEANFILES = ipfixcol-ipfix-input.1
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

