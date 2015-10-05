pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers $(SFLOW_CPPFLAGS) -I$(top_srcdir)/src/utils/conversion

plugins_LTLIBRARIES = ipfixcol-udp-input.la
ipfixcol_udp_input_la_LDFLAGS = -module -avoid-version -shared -L$(top_srcdir)/src/utils/conversion
ipfixcol_udp_input_la_LIBADD = -lrt -lconversion

ipfixcol_udp_input_la_SOURCES = udp_input.c
