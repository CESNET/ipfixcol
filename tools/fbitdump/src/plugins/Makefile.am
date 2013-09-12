ACLOCAL_AMFLAGS = -I m4

pluginsdir = $(datadir)/fbitdump/plugins

plugins_LTLIBRARIES = httprt.la

httprt_la_SOURCES=httprt.c
httprt_la_LDFLAGS= -shared -module -avoid-version

EXTRA_DIST = plugin_header.h