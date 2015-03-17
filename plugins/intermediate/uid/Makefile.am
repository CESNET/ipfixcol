ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS += -I$(top_srcdir)/headers
pluginsdir = $(datadir)/ipfixcol/plugins

sofile = $(pluginsdir)/ipfixcol-uid-inter.so
internalcfg = $(DESTIDR)$(sysconfdir)/ipfixcol/internalcfg.xml

plugins_LTLIBRARIES = ipfixcol-uid-inter.la
ipfixcol_uid_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_uid_inter_la_SOURCES = uid.c

rpmspec = $(PACKAGE_TARNAME).spec
RPMDIR = RPMBUILD

if HAVE_DOC
MANSRC = ipfixcol-uid-inter.dbk
EXTRA_DIST = $(MANSRC)
man_MANS = ipfixcol-uid-inter.1
CLEANFILES = ipfixcol-uid-inter.1
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
	    ipfixconf add -c "$(internalcfg)" -p m -n uid -t uid -s "$(sofile)" -f; \
	fi
