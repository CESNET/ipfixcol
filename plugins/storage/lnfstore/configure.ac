AC_PREREQ([2.60])
# Process this file with autoconf to produce a configure script.
AC_INIT([ipfixcol-lnfstore-output], [0.2.6])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
LT_PREREQ([2.2])
LT_INIT([dlopen disable-static])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([lnfstore.c])
AC_CONFIG_HEADERS([config.h])

# Initialization
CFLAGS="-Wall -std=gnu99"

RELEASE=1
AC_SUBST(RELEASE)

# Set user name and email for packaging purposes 
LBR_SET_CREDENTIALS
LBR_SET_DISTRO([redhat])

############################ Check for programs ################################

# Check for rpmbuil
AC_CHECK_PROG(RPMBUILD, rpmbuild, rpmbuild)

# Check for xsltproc
LBR_CHECK_XSLTPROC

# Check for standard programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

############################ Check for libraries ###############################

### LibXML2 ###
AC_CHECK_LIB([xml2], [main],
    [LIBS="`xml2-config --libs` $LIBS"
    CPPFLAGS="`xml2-config --cflags` $CPPFLAGS"],
    AC_MSG_ERROR([Libxml2 not found ]))

AC_CHECK_LIB([nf], [lnf_open],
             [],
             [AC_MSG_ERROR([libnf library not found])])

AC_CHECK_LIB([bfindex], [create_index],
             [],
             [AC_MSG_ERROR([bfindex library not found])])

###################### Check for configure parameters ##########################
AC_ARG_ENABLE([debug], 
        AC_HELP_STRING([--enable-debug],[turn on more debugging options]),
        [CFLAGS="$CFLAGS -Wextra -g"])

AC_ARG_ENABLE([doc],
        AC_HELP_STRING([--disable-doc],[disable documentation building]))
AM_CONDITIONAL([HAVE_DOC], [test "$enable_doc" != "no"])

######################### Checks for header files ##############################
AC_CHECK_HEADERS([arpa/inet.h inttypes.h string.h unistd.h])

# Check whether we can find headers dir in relative path (git repository)
AS_IF([test -d $srcdir/../../../base/headers], 
	[CPPFLAGS="$CPPFLAGS -I$srcdir/../../../base/headers"]
)

AC_CHECK_HEADERS([ipfixcol.h], , AC_MSG_ERROR([ipfixcol.h header missing. Please install ipfixcol-devel package]), [AC_INCLUDES_DEFAULT])

# Check libnf headers
AC_CHECK_HEADERS([libnf.h], , AC_MSG_ERROR([libnf.h header missing. Please install libnf package]), [AC_INCLUDES_DEFAULT])

# Check libbfindex header
AC_CHECK_HEADERS([bf_index.h], , AC_MSG_ERROR([bf_index.h header missing.]), [AC_INCLUDES_DEFAULT])

######## Checks for typedefs, structures, and compiler characteristics #########
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

######################## Checks for library functions ##########################
AC_CHECK_FUNCS([malloc])
AC_CHECK_FUNCS([realloc])
AC_CHECK_FUNCS([memset])

############################### Set output #####################################

AC_SUBST(RPMBUILD)
if test -z "$RPMBUILD"; then
	AC_MSG_WARN([Due to missing rpmbuild you will not able to generate RPM package.])
fi

AC_SUBST(XSLTPROC)

# generate output
AC_CONFIG_FILES([Makefile
				ipfixcol-lnfstore-output.spec])

# tools makefiles

AC_OUTPUT

AS_IF([test -z "$RPMBUILD"], AC_MSG_WARN([Due to missing rpmbuild you will not able to generate RPM package.]))

AM_COND_IF(HAVE_DOC,
    [AM_COND_IF(HAVE_XSLTPROC, ,
        AC_MSG_ERROR([Missing xsltproc - install it or run with --disable-doc])
    )]
)

# Print final summary
echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix........: $prefix
  Distribution..: $DISTRO
  C Compiler....: $CC $CFLAGS $CPPFLAGS
  Linker........: $LDFLAGS $LIBS
  rpmbuild......: ${RPMBUILD:-NONE}
  Build doc.....: ${enable_doc:-yes}
  xsltproc......: ${XSLTPROC:-NONE}
  xsltmanstyle..: $XSLTMANSTYLE
"
