ACLOCAL_AMFLAGS = -I m4

SUBDIRS = pugixml
AM_CPPFLAGS += -I$(top_srcdir)/pugixml $(SISO_CPPFLAGS)
#-I$(top_srcdir)/../../../base/src/utils/libsiso/
AM_LDFLAGS += $(SISO_LDFLAGS) -module -avoid-version -shared 
 
pluginsdir = $(datadir)/ipfixcol/plugins

sofile = $(pluginsdir)/ipfixcol-nfdump_ext-output.so
internalcfg = $(DESTDIR)$(sysconfdir)/ipfixcol/internalcfg.xml

plugins_LTLIBRARIES = ipfixcol-nfdump_ext-output.la
#ipfixcol_nfdump_ext_output_la_LDFLAGS = $(AM_LDFLAGS) -module -avoid-version -shared 
#-L$(top_srcdir)/../../../base/src/utils/libsiso/.libs/
ipfixcol_nfdump_ext_output_la_SOURCES = \
	nfdump_ext.cpp nfdump_ext.h \
	Storage.cpp Storage.h \
	Translator.cpp Translator.h \
	protocols.h protocols.cpp \
	Sender.cpp Sender.h \
	Printer.cpp Printer.h

ipfixcol_nfdump_ext_output_la_LIBADD = pugixml/libpugixml.la

BUILT_SOURCES = protocols.cpp
CLEANFILES = protocols.cpp
EXTRA_DIST = protocols.cpp

if HAVE_DOC
MANSRC = ipfixcol-nfdump_ext-output.dbk
EXTRA_DIST += $(MANSRC)
man_MANS = ipfixcol-nfdump_ext-output.1
CLEANFILES += ipfixcol-nfdump_ext-output.1
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
	    ipfixconf add -c "$(internalcfg)" -p o -n nfdump_ext -t nfdump_ext -s "$(sofile)" -f; \
	fi

protocols.cpp: /etc/protocols
	awk -f $(top_srcdir)/generate_protocols.awk /etc/protocols > protocols.cpp
