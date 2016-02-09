AM_CPPFLAGS += -I$(top_srcdir)/headers

noinst_LIBRARIES = libprofiles.a

libprofiles_a_CFLAGS = -fPIC
libprofiles_a_SOURCES = \
	Channel.cpp Channel.h \
	profiles.cpp profiles_internal.h \
	Profile.cpp Profile.h \
        filter_wrapper.c filter_wrapper.h \
        ffilter.c ffilter.h \
        ffilter_gram.c ffilter_gram.h \
        ffilter_lex.h ffilter_lex.c

CLEANFILES = ffilter_gram.c ffilter_gram.h ffilter_lex.h ffilter_lex.c
BUILT_SOURCES = ffilter_gram.c ffilter_gram.h ffilter_lex.h ffilter_lex.c


ffilter_gram.h ffilter_gram.c: ffilter_gram.y
        $(BISON) --defines=ffilter_gram.h -o ffilter_gram.c $<

ffilter_lex.h ffilter_lex.c: ffilter_lex.l
        $(FLEX) -o ffilter_lex.c --header-file=ffilter_lex.h $<

EXTRA_DIST = ffilter_gram.y ffilter_lex.l
