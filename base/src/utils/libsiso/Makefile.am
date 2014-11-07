# libsiso is shared library -> api needs to be visible
# visibility in AM_CFLAGS is set on "hidden" -> override flag
AM_CFLAGS += -fvisibility=default

lib_LTLIBRARIES = libsiso.la
libsiso_la_LDFLAGS = -avoid-version -shared
libsiso_la_LIBADD = -lrt

libsiso_la_SOURCES = siso.c
include_HEADERS = siso.h
