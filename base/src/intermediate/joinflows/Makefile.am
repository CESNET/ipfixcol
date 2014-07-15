pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-joinflows-inter.la
ipfixcol_joinflows_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_joinflows_inter_la_LIBADD = -lrt

ipfixcol_joinflows_inter_la_SOURCES = joinflows_ip.c

MANSRC = ipfixcol-joinflows-inter.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-joinflows-inter.1
CLEANFILES = ipfixcol-joinflows-inter.1

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

