ACLOCAL_AMFLAGS = -I m4

pluginsdir = $(datadir)/ipfixcol/plugins
AM_CPPFLAGS = -I$(srcdir)/../../../base/headers $(SFLOW_CPPFLAGS) -I$(top_srcdir)/../../../base/src/utils/conversion

sofile = $(pluginsdir)/ipfixcol-udp-cpg-input.so
internalcfg = $(DESTDIR)$(sysconfdir)/ipfixcol/internalcfg.xml

plugins_LTLIBRARIES = ipfixcol-udp-cpg-input.la
ipfixcol_udp_cpg_input_la_LDFLAGS = -module -avoid-version -shared -L$(top_srcdir)/../../../base/src/utils/conversion
ipfixcol_udp_cpg_input_la_LIBADD = -lrt -lconversion
ipfixcol_udp_cpg_input_la_SOURCES = udp_cpg.c

if HAVE_DOC
MANSRC = ipfixcol-udp-cpg-input.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-udp-cpg-input.1
CLEANFILES = ipfixcol-udp-cpg-input.1
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
	    ipfixconf add -c "$(internalcfg)" -p i -n udp-cpgCollector -t udp-cpgCollector -s "$(sofile)" -f; \
	fi
