ACLOCAL_AMFLAGS = -I m4

ipfixcoldir = $(datadir)/ipfixcol
pluginsdir = $(datadir)/ipfixcol/plugins

sofile = $(pluginsdir)/ipfixcol-unirec-output.so
internalcfg = $(DESTDIR)$(sysconfdir)/ipfixcol/internalcfg.xml

plugins_LTLIBRARIES = ipfixcol-unirec-output.la
ipfixcol_unirec_output_la_LDFLAGS = -module -avoid-version -shared -ltrap
ipfixcol_unirec_output_la_SOURCES = unirec.c unirec.h fast_hash_table.c fast_hash_table.h hashes.h
ipfixcol_unirec_output_la_CFLAGS  = -std=gnu99 -O2

EXTRA_DIST = unirec-elements.txt

if HAVE_DOC
MANSRC = ipfixcol-unirec-output.dbk
man_MANS = ipfixcol-unirec-output.1
EXTRA_DIST += $(MANSRC)
CLEANFILES = ipfixcol-unirec-output.1
endif

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

install-data-hook:
	@if [ -f "$(internalcfg)" ]; then \
	    ipfixconf add -c "$(internalcfg)" -p o -n unirec -t unirec -s "$(sofile)" -f; \
	fi

