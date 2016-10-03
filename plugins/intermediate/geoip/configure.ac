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
AC_INIT([ipfixcol-geoip-inter], [0.0.3])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
AC_GNU_SOURCE

LT_PREREQ([2.2])
LT_INIT([disable-static])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([geoip.c])
dnl AM_CONFIG_HEADER(config.h)

# Initialization
AM_CFLAGS="-Wall -O2 -std=gnu99"
# We need -fPIC to link to some of the libraries
LDFLAGS="$LDFLAGS -fPIC"
AC_LANG(C)

RELEASE=1
AC_SUBST(RELEASE)

# Set user name and email for packaging purposes 
LBR_SET_CREDENTIALS
LBR_SET_DISTRO([redhat])

############################ Check for programs ################################

# Check for rpmbuild
AC_CHECK_PROG(RPMBUILD, rpmbuild, rpmbuild)

# Check for xsltproc
LBR_CHECK_XSLTPROC
AC_SUBST([BUILDREQS])

# Check for standard programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

###################### Check for configure parameters ##########################
AC_ARG_ENABLE([debug], 
        AC_HELP_STRING([--enable-debug],[turn on more debugging options]),
        [AM_CFLAGS="$AM_CFLAGS -Wextra -g"])

AC_ARG_ENABLE([doc],
        AC_HELP_STRING([--disable-doc],[disable documentation building]))
AM_CONDITIONAL([HAVE_DOC], [test "$enable_doc" != "no"])

############################ Check for libraries ###############################

### LibXML2 ###
AC_CHECK_LIB([xml2], [main],
    [LIBS="`xml2-config --libs` $LIBS"
    CPPFLAGS="`xml2-config --cflags` $CPPFLAGS"],
    AC_MSG_ERROR([Libxml2 not found ]))

######################### Checks for header files ##############################
AC_CHECK_HEADERS([float.h netinet/in.h stddef.h stdint.h stdlib.h string.h wchar.h])

# Check whether we can find headers dir in relative path (git repository)
AS_IF([test -d $srcdir/../../../base/headers], 
	[AM_CPPFLAGS+=" -I$srcdir/../../../base/headers"
	BUILD_AGAINST="git"]
)

cppflags_save="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $AM_CPPFLAGS"
AC_CHECK_HEADERS([ipfixcol.h], , AC_MSG_ERROR([ipfixcol.h header missing. Please install ipfixcol-devel package]), [AC_INCLUDES_DEFAULT])
CPPFLAGS="$cppflags_save"

######## Checks for typedefs, structures, and compiler characteristics #########
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

######################## Checks for library functions ##########################
AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([malloc])
AC_CHECK_FUNCS([realloc])
AC_FUNC_STRTOD
AC_CHECK_FUNCS([floor memmove mkdir strchr strstr strtol strtoul])
AC_CHECK_DECL([be64toh], [AC_DEFINE([HAVE_BE64TOH], [1],
                               [Define if macro be64toh exists.])],,
							   [[#include <endian.h>]])

############################### Set output #####################################
# Substitute compiler flags
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_LDFLAGS])

AC_SUBST(RPMBUILD)
if test -z "$RPMBUILD"; then
	AC_MSG_WARN([Due to missing rpmbuild you will not able to generate RPM package.])
fi

AC_SUBST(XSLTPROC)
if test -z "$XSLTPROC"; then
	AC_MSG_WARN([Due to missing xsltproc you will not able to generate MAN pages.])
fi

# generate output
AC_CONFIG_FILES([Makefile
		ipfixcol-geoip-inter.spec])

# tools makefiles

AC_OUTPUT

AS_IF([test -z "$RPMBUILD"], AC_MSG_WARN([Due to missing rpmbuild you will not able to generate RPM package.]))

AM_COND_IF(HAVE_DOC,
    [AM_COND_IF(HAVE_XSLTPROC, ,
        AC_MSG_ERROR([Missing xsltproc - install it or run with --disable-doc])
    )]
)


### GeoIP ###
AC_CHECK_LIB([GeoIP], [GeoIP_open], ,
    AC_MSG_ERROR([libGeoIP not found]))


# Print final summary
echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix........: $prefix
  Distribution..: $DISTRO
  C Compiler....: $CC $AM_CFLAGS $CFLAGS
  Linker........: $AM_LDFLAGS $LDFLAGS $LIBS
  Build against.: ${BUILD_AGAINST:-system}
  rpmbuild......: ${RPMBUILD:-NONE}
  Build doc.....: ${enable_doc:-yes}
  xsltproc......: ${XSLTPROC:-NONE}
  xsltmanstyle..: $XSLTMANSTYLE
"
