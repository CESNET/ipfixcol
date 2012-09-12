ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = -rdynamic

bin_PROGRAMS = fbitconvert

fbitconvert_SOURCES = fbitconvert.c ext_fill.c ext_fill.h ext_parse.c ext_parse.h nffile.h verbose.c

rpmspec = $(PACKAGE_TARNAME).spec
RPMDIR = RPMBUILD

.PHONY: rpm
rpm: dist $(rpmspec)
	@mkdir -p $(RPMDIR)/BUILD $(RPMDIR)/RPMS $(RPMDIR)/SOURCES $(RPMDIR)/SPECS $(RPMDIR)/SRPMS;
	mv $(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz $(RPMDIR)/SOURCES/$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-$(RELEASE).tar.gz
	$(RPMBUILD) -ba $(rpmspec) \
		--define "_topdir `pwd`/$(RPMDIR)";

clean-local: 
	rm -rf RPMBUILD
