AM_CPPFLAGS += \
	-fPIC \
	-I$(top_srcdir)/headers \
	-Ilibnf-ffilter \
	-I$(top_srcdir)/src #last one is for config.h in ffilter
AM_CFLAGS += -fPIC

noinst_LIBRARIES = libprofiles.a

libprofiles_a_SOURCES = \
	Channel.cpp Channel.h \
	profiles.cpp profiles_internal.h \
	Profile.cpp Profile.h \
	libnf-ffilter/ffilter.c libnf-ffilter/ffilter.h \
	libnf-ffilter/ffilter_internal.h\
	ffilter_gram.c ffilter_gram.h \
	ffilter_lex.h ffilter_lex.c \
	filter_wrapper.c filter_wrapper.h

CLEANFILES = ffilter_gram.c ffilter_gram.h ffilter_lex.h ffilter_lex.c
BUILT_SOURCES = ffilter_gram.c ffilter_gram.h ffilter_lex.h ffilter_lex.c


ffilter_gram.h ffilter_gram.c: libnf-ffilter/ffilter_gram.y
	$(BISON) --defines=ffilter_gram.h -o ffilter_gram.c $<

#TODO : fix ffilter_lex.c rule //What does not work ?
ffilter_lex.c ffilter_lex.h: libnf-ffilter/ffilter_lex.l
	$(FLEX) --header-file=ffilter_lex.h --outfile=ffilter_lex.c $<
	@test -f lex.yy.c && mv lex.yy.c ffilter_lex.c

EXTRA_DIST = libnf-ffilter/ffilter_gram.y libnf-ffilter/ffilter_lex.l
