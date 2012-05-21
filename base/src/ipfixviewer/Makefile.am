pluginsdir = $(pkgdatadir)/plugins
dist_pkgdata_DATA = ipfixviewer_startup.xml
bin_SCRIPTS = ipfixviewer

AM_CPPFLAGS = -I$(top_srcdir)/headers

EXTRA_DIST = ipfixviewer.in

plugins_LTLIBRARIES = ipfixcol-ipfixviewer-output.la
ipfixcol_ipfixviewer_output_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_ipfixviewer_output_la_SOURCES = ipfixcol-ipfixviewer-output.c

editbindir = sed -e 's|@bindir[@]|$(bindir)|g'
editpkgdatadir = sed -e 's|@pkgdatadir[@]|$(pkgdatadir)|g'
ipfixviewer: ipfixviewer.in
	$(editbindir) $(srcdir)/$< | $(editpkgdatadir) > $@

