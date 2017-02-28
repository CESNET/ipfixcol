pluginsdir = $(datadir)/ipfixcol/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-timenow-inter.la
ipfixcol_timenow_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_timenow_inter_la_SOURCES = timenow.c

if HAVE_DOC
MANSRC = ipfixcol-timenow-inter.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-timenow-inter.1
CLEANFILES = ipfixcol-timenow-inter.1
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
