ACLOCAL_AMFLAGS = -I m4

pluginsdir = $(datadir)/ipfixcol/plugins

sofile = $(pluginsdir)/ipfixcol-fastbit_compression-output.so
internalcfg = $(DESTDIR)$(sysconfdir)/ipfixcol/internalcfg.xml

plugins_LTLIBRARIES = ipfixcol-fastbit_compression-output.la
ipfixcol_fastbit_compression_output_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_fastbit_compression_output_la_SOURCES = ipfixcol_fastbit.cpp ipfixcol_fastbit.h configuration.cpp configuration.h database.cpp database.h compression.h compression.cpp util.cpp util.h types.h

if HAVE_DOC
MANSRC = ipfixcol-fastbit_compression-output.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-fastbit_compression-output.1
CLEANFILES = ipfixcol-fastbit_compression-output.1
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
	    ipfixconf add -c "$(internalcfg)" -p o -n fastbit_compression -t fastbitc -s "$(sofile)" -f; \
	fi

uninstall-hook:
	@if [ -f "$(internalcfg)" ]; then \
	    ipfixconf remove -c "$(internalcfg)" -p o -n fastbit_compression; \
	fi

