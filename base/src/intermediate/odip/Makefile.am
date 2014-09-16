pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-odip-inter.la
ipfixcol_odip_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_odip_inter_la_LIBADD = -lrt

ipfixcol_odip_inter_la_SOURCES = odip.c

if HAVE_DOC
MANSRC = ipfixcol-odip-inter.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-odip-inter.1
CLEANFILES = ipfixcol-odip-inter.1
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

