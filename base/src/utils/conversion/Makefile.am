AM_CPPFLAGS += -I$(top_srcdir)/headers $(SFLOW_CPPFLAGS)

AM_CFLAGS += -fPIC

noinst_LIBRARIES = libconversion.a
libconversion_a_SOURCES = convert.c convert.h
if ENABLE_SFLOW
libconversion_a_SOURCES += sflow.c
endif
