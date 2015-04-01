ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS += -I$(top_srcdir)/headers
pluginsdir = $(datadir)/ipfixcol/plugins

sofile = $(pluginsdir)/ipfixcol-geoip-inter.so
internalcfg = $(DESTDIR)$(sysconfdir)/ipfixcol/internalcfg.xml

plugins_LTLIBRARIES = ipfixcol-geoip-inter.la
ipfixcol_geoip_inter_la_LDFLAGS = -module -avoid-version -shared -lGeoIP
ipfixcol_geoip_inter_la_SOURCES = geoip.c countrycode.c countrycode.h

rpmspec = $(PACKAGE_TARNAME).spec
RPMDIR = RPMBUILD

# create geoip.h with proper paths to database files
EXTRA_DIST = geoip.h.in
CLEANFILES = geoip.h

# Build these before everyting else
BUILT_SOURCES = geoip.h

EXTRA_DIST += GeoIPv6.dat
pkgdata_DATA = GeoIPv6.dat

if HAVE_DOC
MANSRC = ipfixcol-geoip-inter.dbk
EXTRA_DIST += $(MANSRC)
man_MANS = ipfixcol-geoip-inter.1
CLEANFILES += ipfixcol-geoip-inter.1
endif

geoip.h: geoip.h.in config.status
	sed 's|@pkgdatadir[@]|$(pkgdatadir)|g' $< > $@

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
	    ipfixconf add -c "$(internalcfg)" -p m -n geoip -t geoip -s "$(sofile)" -f; \
	fi
