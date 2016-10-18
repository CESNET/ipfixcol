pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers -fno-strict-aliasing

plugins_LTLIBRARIES = ipfixcol-anonymization-inter.la
ipfixcol_anonymization_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_anonymization_inter_la_LIBADD = -lrt

ipfixcol_anonymization_inter_la_SOURCES = anonymization_ip.c Crypto-PAn/panonymizer.c Crypto-PAn/rijndael.c Crypto-PAn/panonymizer.h Crypto-PAn/rijndael.h

if HAVE_DOC
MANSRC = ipfixcol-anonymization-inter.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-anonymization-inter.1
CLEANFILES = ipfixcol-anonymization-inter.1
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

