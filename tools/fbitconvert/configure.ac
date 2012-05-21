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
AC_INIT([fbitconvert], [1.0.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([fbitconvert.c])
AC_CONFIG_HEADERS([config.h])

# Initialization
CFLAGS="-Wall"

RELEASE=1
AC_SUBST(RELEASE)

# Set user name and email for packaging purposes 
LBR_SET_CREDENTIALS

############################ Check for programs ################################

# Check for rpmbuil
AC_CHECK_PROG(RPMBUILD, rpmbuild, rpmbuild)

# Check for standard programs
AC_PROG_CC
AC_PROG_INSTALL

############################ Check for libraries ###############################

### dynamic linker ###
AC_CHECK_LIB([dl], [dlopen],,
        AC_MSG_ERROR([Required library dl missing]))

###################### Check for configure parameters ##########################
AC_ARG_ENABLE([debug], 
        AC_HELP_STRING([--enable-debug],[turn on more debugging options]),
        [CFLAGS="$CFLAGS -Wextra -g"])
        
######################### Checks for header files ##############################
AC_CHECK_HEADERS([stddef.h stdint.h stdlib.h unistd.h])

# Check whether we can find headers dir in relative path (git repository)
AS_IF([test -d $srcdir/../../base/headers], 
	[CPPFLAGS="$CPPFLAGS -I$srcdir/../../base/headers"]
)

AC_CHECK_HEADERS([ipfixcol.h], , AC_MSG_ERROR([ipfixcol.h header missing. Please install ipfixcol-devel package]), [AC_INCLUDES_DEFAULT])

######## Checks for typedefs, structures, and compiler characteristics #########
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

######################## Checks for library functions ##########################
AC_FUNC_MALLOC

############################### Set output #####################################

AC_CONFIG_FILES([fbitconvert.spec
				Makefile])

AC_OUTPUT

# print warnings

AS_IF([test -z "$RPMBUILD"],
	AC_MSG_WARN([Due to missing rpmbuild you will not able to generate RPM package.])
)

# Print final summary
echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix........: $prefix
  Debug build...: ${enable_debug:-no}
  C Compiler....: $CC $CFLAGS $CPPFLAGS
  Linker........: $LDFLAGS $LIBS
  rpmbuild......: ${RPMBUILD:-NONE}
"
