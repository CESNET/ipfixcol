#
# Copyright (c) 2011 CESNET
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
AC_INIT([ipfixcol-unirec-output], [0.2.4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
LT_PREREQ([2.2])
LT_INIT([dlopen disable-static])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([unirec.c])
AC_CONFIG_HEADERS([config.h])

# Initialization
CFLAGS="-Wall"

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
    CFLAGS="`xml2-config --cflags` $CFLAGS"],
    AC_MSG_ERROR([Libxml2 not found ]))

### libtrap ###
AC_SEARCH_LIBS([trap_init],[trap],,
    AC_MSG_ERROR([libtrap not found; can be retrieved from https://www.liberouter.org/technologies/nemea/]))

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

# Check libtrap headers
AC_CHECK_HEADERS([libtrap/trap.h], , AC_MSG_ERROR([libtrap/trap.h header missing. Please install libtrap-devel package]), [AC_INCLUDES_DEFAULT])

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
				ipfixcol-unirec-output.spec])

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
