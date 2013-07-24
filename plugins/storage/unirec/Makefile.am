ACLOCAL_AMFLAGS = -I m4

ipfixcoldir = $(datadir)/ipfixcol
pluginsdir = $(datadir)/ipfixcol/plugins

plugins_LTLIBRARIES = ipfixcol-unirec-output.la
ipfixcol_unirec_output_la_LDFLAGS = -module -avoid-version -shared -ltrap
ipfixcol_unirec_output_la_SOURCES = unirec.c unirec.h

MANSRC = ipfixcol-unirec-output.dbk
man_MANS = ipfixcol-unirec-output.1

EXTRA_DIST = $(MANSRC) unirec-elements.txt
CLEANFILES = ipfixcol-unirec-output.1

ipfixcol_DATA = unirec-elements.txt

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
