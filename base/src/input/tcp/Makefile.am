pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS += -I$(top_srcdir)/headers $(TLS_CPPFLAGS) -I$(top_srcdir)/src/utils/conversion
AM_CFLAGS += $(TLS_CFLAGS)

plugins_LTLIBRARIES = ipfixcol-tcp-input.la
ipfixcol_tcp_input_la_LDFLAGS = -module -avoid-version -shared -L$(top_srcdir)/src/utils/conversion
ipfixcol_tcp_input_la_LIBADD = $(TLS_LIBS) -lrt -lconversion

ipfixcol_tcp_input_la_SOURCES = tcp_input.c
