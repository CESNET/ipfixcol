pluginsdir = $(pkgdatadir)/plugins
AM_CPPFLAGS = -I$(top_srcdir)/headers

plugins_LTLIBRARIES = ipfixcol-anonymization-inter.la
ipfixcol_anonymization_inter_la_LDFLAGS = -module -avoid-version -shared
ipfixcol_anonymization_inter_la_LIBADD = -lrt

ipfixcol_anonymization_inter_la_SOURCES = anonymization_ip.c Crypto-PAn/panonymizer.c Crypto-PAn/rijndael.c
