ACLOCAL_AMFLAGS = -I m4

ipfixcoldir = $(datadir)/ipfixcol 
pluginsdir = $(datadir)/ipfixcol/plugins

sofile = $(pluginsdir)/ipfixcol-lnfstore-output.so
internalcfg = $(DESTDIR)$(sysconfdir)/ipfixcol/internalcfg.xml

ipfixcol_lnfstore_output_la_LDFLAGS = -module -avoid-version -shared 
ipfixcol_lnfstore_output_la_SOURCES = \
	lnfstore.c lnfstore.h \
	storage.c storage.h \
	translator.c translator.h \
	bitset.c bitset.h

plugins_LTLIBRARIES = ipfixcol-lnfstore-output.la

if HAVE_DOC
MANSRC = ipfixcol-lnfstore-output.dbk
man_MANS = ipfixcol-lnfstore-output.1
EXTRA_DIST = $(MANSRC) # += error here
CLEANFILES = ipfixcol-lnfstore-output.1 #and here  
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
	    ipfixconf add -c "$(internalcfg)" -p o -n lnfstore -t lnfstore -s "$(sofile)" -f; \
	fi

