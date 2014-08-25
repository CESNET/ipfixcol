pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-dummy-output.la
ipfixcol_dummy_output_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_dummy_output_la_SOURCES = dummy_output.c

if HAVE_DOC
MANSRC = ipfixcol-dummy-output.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-dummy-output.1
CLEANFILES = ipfixcol-dummy-output.1
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

