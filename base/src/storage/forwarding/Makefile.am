pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-forwarding-output.la
ipfixcol_forwarding_output_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_forwarding_output_la_LIBADD = -lsctp -lrt

ipfixcol_forwarding_output_la_SOURCES = forwarding.c
