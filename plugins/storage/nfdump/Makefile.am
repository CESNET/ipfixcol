ACLOCAL_AMFLAGS = -I m4

SUBDIRS = pugixml
AM_CPPFLAGS = -I$(top_srcdir)/pugixml

pluginsdir = $(datadir)/ipfixcol/plugins

sofile = $(pluginsdir)/ipfixcol-nfdump-output.so
internalcfg = $(DESTDIR)$(sysconfdir)/ipfixcol/internalcfg.xml

LICENSE = ipfixcol-nfdump-output.txt
plugins_DATA = ipfixcol-nfdump-output.txt

plugins_LTLIBRARIES = ipfixcol-nfdump-output.la
ipfixcol_nfdump_output_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_nfdump_output_la_SOURCES = nfstore.cpp nfstore.h record_map.cpp record_map.h extensions.cpp extensions.h nffile.h config_struct.h
ipfixcol_nfdump_output_la_LIBADD = pugixml/libpugixml.la

if HAVE_DOC
MANSRC = ipfixcol-nfdump-output.dbk
EXTRA_DIST = $(MANSRC) $(LICENSE)
man_MANS = ipfixcol-nfdump-output.1
CLEANFILES = ipfixcol-nfdump-output.1
endif

rpmspec = $(PACKAGE_TARNAME).spec
RPMDIR = RPMBUILD

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


.PHONY: rpm
rpm: dist $(rpmspec)
	@mkdir -p $(RPMDIR)/BUILD $(RPMDIR)/RPMS $(RPMDIR)/SOURCES $(RPMDIR)/SPECS $(RPMDIR)/SRPMS;
	mv $(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz $(RPMDIR)/SOURCES/$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(RELEASE).tar.gz
	$(RPMBUILD) -ba $(rpmspec) \
		--define "_topdir `pwd`/$(RPMDIR)";

clean-local: 
	rm -rf RPMBUILD

install-data-hook:
	@if [ -f "$(internalcfg)" ]; then \
	    ipfixconf add -c "$(internalcfg)" -p o -n nfdump -t nfdump -s "$(sofile)" -f; \
	fi

