pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-filter-inter.la
ipfixcol_filter_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_filter_inter_la_LIBADD = -lrt

ipfixcol_filter_inter_la_SOURCES = filter.c
