pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers $(SFLOW_CPPFLAGS)

plugins_LTLIBRARIES = ipfixcol-udp-input.la
ipfixcol_udp_input_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_udp_input_la_LIBADD = -lrt

ipfixcol_udp_input_la_SOURCES = udp_input.c convert.c convert.h
if ENABLE_SFLOW
ipfixcol_udp_input_la_SOURCES += sflow.c sflow.h sflowtool.h
endif