EXTRA_DIST = doxyconf.in 

docpkg = $(PACKAGE_TARNAME)-doxy-$(PACKAGE_VERSION).tar.gz

doc: $(docpkg)

$(docpkg): doxygen.stamp
	tar chof - html latex | gzip -9 -c >$@

doxygen.stamp: doxyconf
	$(DOXYGEN) $<
	echo Timestamp > $@

clean-local:
	rm -rf html latex doxywarn.txt doxygen.stamp $(docpkg)

