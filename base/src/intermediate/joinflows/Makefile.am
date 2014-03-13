pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-joinflows-inter.la
ipfixcol_joinflows_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_joinflows_inter_la_LIBADD = -lrt

ipfixcol_joinflows_inter_la_SOURCES = joinflows_ip.c
