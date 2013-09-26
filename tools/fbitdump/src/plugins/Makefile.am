ACLOCAL_AMFLAGS = -I m4

pluginsdir = $(datadir)/fbitdump/plugins

plugins_LTLIBRARIES = httprt.la http_status_code.la dns_rcode.la

httprt_la_SOURCES=httprt.c
httprt_la_LDFLAGS= -shared -module -avoid-version

http_status_code_la_SOURCES=http_status_code.c
http_status_code_la_LDFLAGS= -shared -module -avoid-version

dns_rcode_la_SOURCES=dns_rcode.c
dns_rcode_la_LDFLAGS= -shared -module -avoid-version

EXTRA_DIST = plugin_header.h
