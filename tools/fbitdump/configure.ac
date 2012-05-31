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
AC_INIT([fbitdump], [0.2.4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/fbitdump.cpp])
AC_CONFIG_HEADERS([config.h])

RELEASE=1
AC_SUBST(RELEASE)

# Initialization
AC_LANG(C++)

CXXFLAGS="-Wall -O2 -std=gnu++0x"
CXX="g++"

# Set user name and email for packaging purposes 
LBR_SET_CREDENTIALS
LBR_SET_DISTRO([redhat])

############################ Check for programs ################################

# Check for flex
AC_CHECK_PROG(FLEX, flex, flex)

# Check for rpmbuild
AC_CHECK_PROG(RPMBUILD, rpmbuild, rpmbuild)

# Check for doxygen
AC_CHECK_PROG(DOXYGEN, doxygen, doxygen)
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n $DOXYGEN])

# Check for xsltproc
LBR_CHECK_XSLTPROC

# Check for standard programs
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_PROG_AWK

############################ Check for libraries ###############################

AC_SEARCH_LIBS([fastbit_init], [fastbit],,
    	AC_MSG_ERROR([Required library fastbit missing]))

### dynamic linker ###
AC_SEARCH_LIBS([dlopen], [dl],,
        AC_MSG_ERROR([Required library dl missing]))

############################# Check for files ##################################

AC_CHECK_FILE([/etc/protocols],[PROTOCOLS=yes], [PROTOCOLS=no])

###################### Check for configure parameters ##########################

AC_ARG_ENABLE([debug], 
        AC_HELP_STRING([--enable-debug],[turn on more debugging options]),
        [CPPFLAGS="$CPPFLAGS -DDEBUG"
        CXXFLAGS="$CXXFLAGS -Wextra -g"])

######################### Checks for header files ##############################

AC_HEADER_RESOLV

AC_CHECK_HEADERS([getopt.h iostream regex.h dirent.h fatbit/ibis.h string arpa/inet.h \
	cstdlib cstring unistd.h algorithm cstdio vector set map string fstream \
	float.h inttypes.h libintl.h malloc.h netdb.h stddef.h stdint.h stdlib.h string.h sys/socket.h wchar.h])

AC_CHECK_HEADERS([fastbit/ibis.h],,AC_MSG_ERROR([fastbit/ibis.h header missing. Please install fastbit-devel package]))

######## Checks for typedefs, structures, and compiler characteristics #########AC_HEADER_STDBOOL
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
AC_CHECK_TYPES([ptrdiff_t])

######################## Checks for library functions ##########################
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([htonl inet_ntop gmtime regcomp getline floor memmove memset regcomp select strchr strdup strerror strrchr strstr strtol strtoul strverscmp])

############################### Set output #####################################

# generate output
AC_CONFIG_FILES([Makefile
				src/Makefile
				3rdparty/Makefile
				man/Makefile
				doc/Makefile
				fbitdump.spec])

AC_OUTPUT

# Print warings
AS_IF([test -z "$FLEX"], AC_MSG_WARN([Due to missing flex you will not be able to generate scanner from sources.]))

AS_IF([test "x$PROTOCOLS" = xno], AC_MSG_WARN([Due to missing /etc/protocols you will not be able to generate protocols.cpp]))

AS_IF([test -z "$AWK"], AC_MSG_WARN([Due to missing awk you will not be able to generate protocols.cpp from /etc/protocols]))

AM_COND_IF(HAVE_DOXYGEN,
	AC_CONFIG_FILES([doc/doxyconf]), 
	AC_MSG_WARN([Due to missing doxygen you will not able to generate Doxygen documentation.]))

AS_IF([test -z "$RPMBUILD"], AC_MSG_WARN([Due to missing rpmbuild you will not able to generate RPM package.]))

AM_COND_IF(HAVE_XSLTPROC,, AC_MSG_WARN([Due to missing xsltproc you will not able to generate MAN pages.]))

# Print final summary
echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix........: $prefix
  Distribution..: $DISTRO
  Debug build...: ${enable_debug:-no}
  C++ Compiler..: $CXX $CXXFLAGS $CPPFLAGS
  Linker........: $LDFLAGS $LIBS
  rpmbuild......: ${RPMBUILD:-NONE}
  xsltproc......: ${XSLTPROC:-NONE}
  xsltmanstyle..: $XSLTMANSTYLE
  flex..........: ${FLEX:-NONE}
  awk...........: $AWK
  /etc/protocols: $PROTOCOLS
  Doxygen.......: ${DOXYGEN:-NONE}
"
