ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = tools fbitdump.xml.template
pkgdata_DATA = fbitdump.xml

rpmspec = $(PACKAGE_TARNAME).spec
RPMDIR = RPMBUILD

SUBDIRS = 3rdparty src src/plugins

if HAVE_DOC
SUBDIRS += man \
		doc
endif

.PHONY: doc
doc: 
	(cd doc && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

.PHONY: rpm
rpm: dist $(rpmspec)
	@mkdir -p $(RPMDIR)/BUILD $(RPMDIR)/RPMS $(RPMDIR)/SOURCES $(RPMDIR)/SPECS $(RPMDIR)/SRPMS;
	mv $(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz $(RPMDIR)/SOURCES/$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(RELEASE).tar.gz
	$(RPMBUILD) -ba $(rpmspec) \
		--define "_topdir `pwd`/$(RPMDIR)";

clean-local: 
	rm -rf RPMBUILD

fbitdump.xml: fbitdump.xml.template
	sed 's|@pkgdatadir[@]|$(pkgdatadir)|g' $< > $@
