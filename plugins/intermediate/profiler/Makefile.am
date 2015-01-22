ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS += -I$(top_srcdir)/headers
pluginsdir = $(datadir)/ipfixcol/plugins

sofile = $(pluginsdir)/ipfixcol-profiler-inter.so
internalcfg = $(DESTIDR)$(sysconfdir)/ipfixcol/internalcfg.xml

plugins_LTLIBRARIES = ipfixcol-profiler-inter.la
ipfixcol_profiler_inter_la_LDFLAGS = -module -avoid-version -shared 
ipfixcol_profiler_inter_la_SOURCES = profiler.cpp profiler.h filter.c filter.h Organization.cpp Organization.h Rule.cpp Rule.h parser.c parser.h scanner.c scanner.h

CLEANFILES = parser.h parser.c scanner.h scanner.c
BUILT_SOURCES = parser.h parser.c scanner.h scanner.c

parser.h parser.c: parser.y
	$(BISON) --defines=parser.h -o parser.c $<

scanner.h scanner.c: scanner.l
	$(FLEX) -o scanner.c --header-file=scanner.h $<

EXTRA_DIST = parser.y scanner.l

if HAVE_DOC
SUBDIRS = doxygen
MANSRC = ipfixcol-profiler-inter.dbk
EXTRA_DIST += $(MANSRC)
man_MANS = ipfixcol-profiler-inter.1
CLEANFILES += ipfixcol-profiler-inter.1
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
	    ipfixconf add -c "$(internalcfg)" -p m -n profiler -t profiler -s "$(sofile)" -f; \
	fi
