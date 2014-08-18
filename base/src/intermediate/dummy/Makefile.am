pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-dummy-inter.la
ipfixcol_dummy_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_dummy_inter_la_LIBADD = -lrt

ipfixcol_dummy_inter_la_SOURCES = dummy_ip.c
