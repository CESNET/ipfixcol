EXTRA_DIST = doxyfile.in

docpkg = $(PACKAGE_TARNAME)-doxy-$(PACKAGE_VERSION).tar.gz

pkgdata_DATA = $(docpkg)

doc: $(docpkg)

$(docpkg): doxygen.stamp
	tar chof - html latex man | gzip -9 -c >$@

doxygen.stamp: doxyfile
	$(DOXYGEN) $<
	echo Timestamp > $@

clean-local:
	rm -rf html latex man doxywarn.txt doxygen.stamp $(docpkg)

