pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir)/src/utils/conversion

plugins_LTLIBRARIES = ipfixcol-sctp-input.la
ipfixcol_sctp_input_la_LDFLAGS = -module -avoid-version -shared -L$(top_srcdir)/src/utils/conversion
ipfixcol_sctp_input_la_LIBADD = -lsctp -lrt -lconversion
ipfixcol_sctp_input_la_SOURCES = sctp_input.c
