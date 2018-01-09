ACLOCAL_AMFLAGS = -I m4

pluginsdir = $(datadir)/fbitdump/plugins

plugins_LTLIBRARIES = httprt.la http_status_code.la sip_method.la sip_msg_type.la dns_rcode.la tls_csuites.la tls_version.la tls_csuites_array.la voip_type.la voip_rtpcodec.la smtp_statuscode.la smtp_command.la mac.la multiplier.la

httprt_la_SOURCES= httprt.c
httprt_la_LDFLAGS= -shared -module -avoid-version

http_status_code_la_SOURCES= http_status_code.c
http_status_code_la_LDFLAGS= -shared -module -avoid-version

sip_method_la_SOURCES= sip_method.c
sip_method_la_LDFLAGS= -shared -module -avoid-version

sip_msg_type_la_SOURCES= sip_msg_type.c
sip_msg_type_la_LDFLAGS= -shared -module -avoid-version

dns_rcode_la_SOURCES= dns_rcode.c
dns_rcode_la_LDFLAGS= -shared -module -avoid-version

tls_csuites_la_SOURCES= tls_csuites.c tls_values.h
tls_csuites_la_LDFLAGS= -shared -module -avoid-version

tls_csuites_array_la_SOURCES= tls_csuites_array.c tls_values.h
tls_csuites_array_la_LDFLAGS= -shared -module -avoid-version

tls_version_la_SOURCES= tls_version.c tls_values.h
tls_version_la_LDFLAGS= -shared -module -avoid-version

voip_type_la_SOURCES= voip_type.c
voip_type_la_LDFLAGS= -shared -module -avoid-version

voip_rtpcodec_la_SOURCES= voip_rtpcodec.c
voip_rtpcodec_la_LDFLAGS= -shared -module -avoid-version

smtp_statuscode_la_SOURCES= smtp_statuscode.c
smtp_statuscode_la_LDFLAGS= -shared -module -avoid-version

smtp_command_la_SOURCES= smtp_command.c
smtp_command_la_LDFLAGS= -shared -module -avoid-version

mac_la_SOURCES= mac.c
mac_la_LDFLAGS= -shared -module -avoid-version

multiplier_la_SOURCES= multiplier.c
multiplier_la_LDFLAGS= -shared -module -avoid-version

EXTRA_DIST = plugin_header.h
