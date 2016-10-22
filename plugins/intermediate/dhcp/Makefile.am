ACLOCAL_AMFLAGS = -I m4

pluginsdir = $(datadir)/ipfixcol/plugins

sofile = $(pluginsdir)/ipfixcol-dhcp-inter.so
internalcfg = $(DESTDIR)$(sysconfdir)/ipfixcol/internalcfg.xml

plugins_LTLIBRARIES = ipfixcol-dhcp-inter.la
ipfixcol_dhcp_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_dhcp_inter_la_SOURCES = dhcp.c

rpmspec = $(PACKAGE_TARNAME).spec
RPMDIR = RPMBUILD

#if HAVE_DOC
MANSRC = ipfixcol-dhcp-inter.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-dhcp-inter.1
CLEANFILES = ipfixcol-dhcp-inter.1
#endif

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

.PHONY: doc
doc:
	(cd doxygen && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

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
	    ipfixconf add -c "$(internalcfg)" -p m -n dhcp -t dhcp -s "$(sofile)" -f; \
	fi
