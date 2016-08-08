pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-forwarding-output.la
ipfixcol_forwarding_output_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_forwarding_output_la_SOURCES = \
	forwarding.c \
	sender.c sender.h \
	configuration.c configuration.h \
	destination.c destination.h \
	packet.c packet.h \
	templates.c templates.h

if HAVE_DOC
MANSRC = ipfixcol-forwarding-output.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-forwarding-output.1
CLEANFILES = ipfixcol-forwarding-output.1
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

