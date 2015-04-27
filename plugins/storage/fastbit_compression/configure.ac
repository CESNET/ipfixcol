#
# Copyright (c) 2015 CESNET
#
# LICENSE TERMS
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name of the Company nor the names of its contributors
#    may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# ALTERNATIVELY, provided that this notice is retained in full, this
# product may be distributed under the terms of the GNU General Public
# License (GPL) version 2 or later, in which case the provisions
# of the GPL apply INSTEAD OF those given above.
#
# This software is provided ``as is'', and any express or implied
# warranties, including, but not limited to, the implied warranties of
# merchantability and fitness for a particular purpose are disclaimed.
# In no event shall the company or contributors be liable for any
# direct, indirect, incidental, special, exemplary, or consequential
# damages (including, but not limited to, procurement of substitute
# goods or services; loss of use, data, or profits; or business
# interruption) however caused and on any theory of liability, whether
# in contract, strict liability, or tort (including negligence or
# otherwise) arising in any way out of the use of this software, even
# if advised of the possibility of such damage.
#
# $Id$
#

AC_PREREQ([2.60])
# Process this file with autoconf to produce a configure script.
AC_INIT([ipfixcol-fastbit_compression-output], [0.1.1])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
LT_PREREQ([2.2])
LT_INIT([dlopen disable-static])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([ipfixcol_fastbit.cpp])
AC_CONFIG_HEADERS([config.h])

# Initialization
AM_CXXFLAGS="-Wall"

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
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_LANG([C++])
LBR_SET_CXXSTD
AM_CXXFLAGS="$AM_CXXFLAGS $CXXSTD"
###################### Check for configure parameters ##########################
AC_ARG_ENABLE([debug], 
	AC_HELP_STRING([--enable-debug],[turn on more debugging options]),
	[AM_CXXFLAGS="$AM_CXXFLAGS -Wextra -g"])

AC_ARG_ENABLE([doc],
	AC_HELP_STRING([--disable-doc],[disable documentation building]))
AM_CONDITIONAL([HAVE_DOC], [test "$enable_doc" != "no"])

AC_ARG_ENABLE([gzip],
	AC_HELP_STRING([--disable-gzip],[disable support for gzip compression.]),
	[enable_gzip=no],
	[enable_gzip=yes])

AC_ARG_ENABLE(bzip,
	AC_HELP_STRING([--disable-bzip],[disable support for bzip2 compression.]),
	[enable_bzip=no],
	[enable_bzip=yes])

############################ Check for libraries ###############################
### LibXML2 ###
AC_CHECK_LIB([xml2], [main],
    [LIBS="`xml2-config --libs` $LIBS"
    AM_CXXFLAGS="`xml2-config --cflags` $CFLAGS"],
    AC_MSG_ERROR([Libxml2 not found ]))

AC_SEARCH_LIBS([fastbit_init], [fastbit],,
	AC_MSG_ERROR([Required library libfastbit is missing]))

AS_IF([test "x$enable_gzip" = "xyes"],
AC_CHECK_LIB([z], [deflate],,
	AC_MSG_ERROR([Required library libz is missing])))

AS_IF([test "x$enable_bzip" = "xyes"],
AC_CHECK_LIB([bz2], [BZ2_bzWrite],,
	AC_MSG_ERROR([Required library libbz2 is missing])))

######################### Checks for header files ##############################
AC_CHECK_HEADERS([arpa/inet.h limits.h stdint.h stdlib.h string.h unistd.h])

# use current CXX flags
my_save_cxxflags="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $CXXSTD $AM_CXXFLAGS"

AC_CHECK_HEADERS([libxml/xmlversion.h], , AC_MSG_ERROR([libxml headers missing. Please install libxml2-devel package]), [AC_INCLUDES_DEFAULT])

# Check whether we can find headers dir in relative path (git repository)
AS_IF([test -d $srcdir/../../../base/headers], 
	[CPPFLAGS="$CPPFLAGS -I$srcdir/../../../base/headers"]
)

AC_CHECK_HEADERS([ipfixcol.h], , AC_MSG_ERROR([ipfixcol.h header missing. Please install ipfixcol-devel package]), [AC_INCLUDES_DEFAULT])


AC_CHECK_HEADERS([fastbit/ibis.h],,AC_MSG_ERROR([fastbit/ibis.h header missing. Please install fastbit-devel package]))

# restore CXX flags
CXXFLAGS="$my_save_cflags"


######## Checks for typedefs, structures, and compiler characteristics #########
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

######################## Checks for library functions ##########################
AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([malloc])
AC_CHECK_FUNCS([realloc])
AC_FUNC_STRTOD
AC_CHECK_FUNCS([localtime_r mkdir strdup strerror])

# Old libz versions do not have gzclose_w function. We can substitute with generic gzclose
AS_IF([test "x$enable_gzip" = "xyes"],
	AC_CHECK_FUNC([gzclose_w],, [AC_DEFINE([gzclose_w], [gzclose], [Substitute missing gzclose_w function])])
)

############################### Set output #####################################
# Substitute compiler flags
AC_SUBST([AM_CXXFLAGS])

AC_SUBST(RPMBUILD)
AS_IF([test -z "$RPMBUILD"], AC_MSG_WARN([Due to missing rpmbuild you will not able to generate RPM package.]))

AC_SUBST(XSLTPROC)
AS_IF([test -z "$XSLTPROC"], AC_MSG_WARN([Due to missing xsltproc you will not able to generate MAN pages.]))

# generate output
AC_CONFIG_FILES([Makefile
		ipfixcol-fastbit_compression-output.spec])

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
  C++ Compiler..: $CXX $AM_CXXFLAGS $CXXFLAGS $CPPFLAGS
  Linker........: $LDFLAGS $LIBS
  gzip..........: $enable_gzip
  bzip2.........: $enable_bzip
  rpmbuild......: ${RPMBUILD:-NONE}
  Build doc.....: ${enable_doc:-yes}
  xsltproc......: ${XSLTPROC:-NONE}
  xsltmanstyle..: $XSLTMANSTYLE
"
