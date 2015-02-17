ACLOCAL_AMFLAGS = -I m4
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ipfixcol.pc
rpmspec = $(PACKAGE_TARNAME).spec
RPMDIR = RPMBUILD

SUBDIRS = src config headers 

if HAVE_DOC
SUBDIRS +=  documentation/man \
	    documentation/doxygen
endif

EXTRA_DIST = README.md tests

.PHONY: doc
doc: 
	(cd documentation/doxygen && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1

.PHONY: rpm
rpm: dist $(rpmspec)
	@mkdir -p $(RPMDIR)/BUILD $(RPMDIR)/RPMS $(RPMDIR)/SOURCES $(RPMDIR)/SPECS $(RPMDIR)/SRPMS;
	mv $(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz $(RPMDIR)/SOURCES/$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(RELEASE).tar.gz
	$(RPMBUILD) -ba $(rpmspec) \
		--define "_topdir `pwd`/$(RPMDIR)";

clean-local: 
	rm -rf RPMBUILD

.PHONY: test
test:
	$(MAKE) $(AM_MAKEFLAGS)
	(cd tests/ipfixcol_test/ && ./test.sh) || exit 1 

dist-hook:
	rm -rf $(distdir)/tests/ipfixcol_test/configs/internalcfg.xml
