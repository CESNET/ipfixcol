pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-hooks-inter.la
ipfixcol_hooks_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_hooks_inter_la_LIBADD = -lrt

ipfixcol_hooks_inter_la_SOURCES = hooks.c

if HAVE_DOC
MANSRC = ipfixcol-hooks-inter.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-hooks-inter.1
CLEANFILES = ipfixcol-hooks-inter.1
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

