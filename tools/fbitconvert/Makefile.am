ACLOCAL_AMFLAGS         = -I m4

rpmspec = $(PACKAGE_TARNAME).spec
RPMDIR = RPMBUILD

dist_pkgdata_DATA = fbitconvert_startup.xml


SUBDIRS = src

if HAVE_DOC
SUBDIRS += man
endif

.PHONY: rpm
rpm: dist $(rpmspec)
	@mkdir -p $(RPMDIR)/BUILD $(RPMDIR)/RPMS $(RPMDIR)/SOURCES $(RPMDIR)/SPECS $(RPMDIR)/SRPMS;
	mv $(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz $(RPMDIR)/SOURCES/$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(RELEASE).tar.gz
	$(RPMBUILD) -ba $(rpmspec) \
		--define "_topdir `pwd`/$(RPMDIR)";

clean-local: 
	rm -rf RPMBUILD
