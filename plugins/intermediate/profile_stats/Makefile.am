ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS += -I$(top_srcdir)/headers
pluginsdir = $(datadir)/ipfixcol/plugins

sofile = $(pluginsdir)/ipfixcol-profilestats-inter.so
internalcfg = $(DESTIDR)$(sysconfdir)/ipfixcol/internalcfg.xml

plugins_LTLIBRARIES = ipfixcol-profilestats-inter.la
ipfixcol_profilestats_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_profilestats_inter_la_SOURCES = profilestats.cpp stats.h

if HAVE_DOC
MANSRC = ipfixcol-profilestats-inter.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-profilestats-inter.1
CLEANFILES = ipfixcol-profilestats-inter.1
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
		ipfixconf add -c "$(internalcfg)" -p m -n profilestats -t profilestats -s "$(sofile)" -f; \
	fi
