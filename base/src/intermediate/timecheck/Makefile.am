pluginsdir = $(datadir)/ipfixcol/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-timecheck-inter.la
ipfixcol_timecheck_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_timecheck_inter_la_SOURCES = timecheck.c

if HAVE_DOC
MANSRC = ipfixcol-timecheck-inter.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-timecheck-inter.1
CLEANFILES = ipfixcol-timecheck-inter.1
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
