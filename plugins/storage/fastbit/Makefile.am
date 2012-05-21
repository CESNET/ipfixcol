ACLOCAL_AMFLAGS = -I m4

SUBDIRS = pugixml
AM_CPPFLAGS = -I$(top_srcdir)/pugixml
LDADD = pugixml/libpugixml.a

pluginsdir = $(datadir)/ipfixcol/plugins

plugins_LTLIBRARIES = ipfixcol-fastbit-output.la
ipfixcol_fastbit_output_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_fastbit_output_la_SOURCES = fastbit.cpp fastbit_table.cpp fastbit_table.h fastbit_element.cpp fastbit_element.h

MANSRC = ipfixcol-fastbit-output.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-fastbit-output.1
CLEANFILES = ipfixcol-fastbit-output.1

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
