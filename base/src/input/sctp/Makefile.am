pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers $(SFLOW_CPPFLAGS)

plugins_LTLIBRARIES = ipfixcol-sctp-input.la
ipfixcol_sctp_input_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_sctp_input_la_LIBADD = -lsctp -lrt

ipfixcol_sctp_input_la_SOURCES = sctp_input.c convert.c convert.h
if ENABLE_SFLOW
ipfixcol_sctp_input_la_SOURCES += sflow.c sflow.h sflowtool.h
endif
