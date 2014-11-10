AM_CPPFLAGS = -I$(top_srcdir)/headers -I./../libsiso

bin_PROGRAMS = ipfixsend

ipfixsend_LDFLAGS = -L./../libsiso/.libs -lsiso
ipfixsend_SOURCES = ipfixsend.h \
			ipfixsend.c \
			reader.h \
			reader.c \
			sender.h \
			sender.c