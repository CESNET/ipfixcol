ACLOCAL_AMFLAGS         = -I m4

bin_SCRIPTS = fbitconvert
EXTRA_DIST = fbitconvert.in

CLEANFILES = $(bin_SCRIPTS)

editbindir = sed -e 's|@bindir[@]|$(bindir)|g'
editpkgdatadir = sed -e 's|@pkgdatadir[@]|$(pkgdatadir)|g'
editpkgname = sed -e 's|@package[@]|$(PACKAGE_NAME)|g'
editpkgversion = sed -e 's|@version[@]|$(PACKAGE_VERSION)|g'
fbitconvert: fbitconvert.in
	$(editbindir) $(srcdir)/$< | $(editpkgdatadir) | $(editpkgname) | $(editpkgversion) > $@