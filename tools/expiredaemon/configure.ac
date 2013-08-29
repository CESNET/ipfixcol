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
AC_INIT([expiredaemon], [0.1])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
AC_CONFIG_SRCDIR([src/expire.c])
AC_CONFIG_HEADERS([config.h])

RELEASE=1
AC_SUBST(RELEASE)

# Initialization
: ${CFLAGS=""}
AM_CFLAGS="-Wall -O2"
AC_LANG(C)

CC="gcc"

# Set user name and email for packaging purposes 
LBR_SET_CREDENTIALS
LBR_SET_DISTRO([redhat])

############################ Check for programs ################################

# Check for rpmbuild
AC_CHECK_PROG(RPMBUILD, rpmbuild, rpmbuild)

# Check for doxygen
AC_CHECK_PROG(DOXYGEN, doxygen, doxygen)
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n $DOXYGEN])

# Check for xsltproc
LBR_CHECK_XSLTPROC

# Check for standard programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_GNU_SOURCE
############################ Check for libraries ###############################

#AC_SEARCH_LIBS([fastbit_init], [fastbit],,
#    	AC_MSG_ERROR([Required library fastbit missing]))

### dynamic linker ###
AC_SEARCH_LIBS([dlopen], [dl],,
        AC_MSG_ERROR([Required library dl missing]))

############################# Check for files ##################################

###################### Check for configure parameters ##########################

AC_ARG_ENABLE([debug], 
        AC_HELP_STRING([--enable-debug],[turn on more debugging options]),
        [AM_CPPFLAGS="$AM_CPPFLAGS -DDEBUG"
        AM_CFLAGS="$AM_CFLAGS -Wextra -g -O0"])

# Substitute compiler flags
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
######################### Checks for header files ##############################

AC_CHECK_HEADERS([fcntl.h inttypes.h limits.h stdint.h stdlib.h string.h syslog.h unistd.h])

#AC_CHECK_HEADERS([fastbit/ibis.h],,AC_MSG_ERROR([fastbit/ibis.h header missing. Please install fastbit-devel package]))

######## Checks for typedefs, structures, and compiler characteristics #########AC_HEADER_STDBOOL
AC_TYPE_UINT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

######################## Checks for library functions ##########################
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([mkfifo rmdir strdup strerror strrchr])
############################### Set output #####################################

# generate output
AC_CONFIG_FILES([Makefile
				src/Makefile
				man/Makefile
				doc/Makefile
				expiredaemon.spec])

AM_COND_IF(HAVE_DOXYGEN, AC_CONFIG_FILES([doc/doxyconf]))

AC_OUTPUT

# Print warings

AM_COND_IF(HAVE_DOXYGEN,, AC_MSG_WARN([Due to missing doxygen you will not able to generate Doxygen documentation.]))

AS_IF([test -z "$RPMBUILD"], AC_MSG_WARN([Due to missing rpmbuild you will not able to generate RPM package.]))

AM_COND_IF(HAVE_XSLTPROC,, AC_MSG_WARN([Due to missing xsltproc you will not able to generate MAN pages.]))

# Print final summary
echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix........: $prefix
  Distribution..: $DISTRO
  Debug build...: ${enable_debug:-no}
  C Compiler....: $CC $AM_CFLAGS $CFLAGS $AM_CPPFLAGS $CPPFLAGS
  Linker........: $LDFLAGS $LIBS
  rpmbuild......: ${RPMBUILD:-NONE}
  xsltproc......: ${XSLTPROC:-NONE}
  xsltmanstyle..: $XSLTMANSTYLE
  Doxygen.......: ${DOXYGEN:-NONE}
"
