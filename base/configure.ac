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
AC_INIT([ipfixcol], [0.7.1])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability subdir-objects])
LT_PREREQ([2.2])
LT_INIT([dlopen disable-static])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/ipfixcol.c])
AC_CONFIG_HEADERS([pkgconfig.h])

# Initialization
AM_CFLAGS="-Wall -O2 -std=gnu99"
AM_CPPFLAGS="-std=c++11"

RELEASE=1
AC_SUBST(RELEASE)

# Set user name and email for packaging purposes 
LBR_SET_CREDENTIALS
LBR_SET_DISTRO([redhat])

###################### Check for configure parameters ##########################
AC_ARG_ENABLE([debug], 
        AC_HELP_STRING([--enable-debug],[turn on more debugging options]),
        [AM_CFLAGS="$AM_CFLAGS -Wextra -g"
        AM_CPPFLAGS="$AM_CPPFLAGS -DDEBUG"])
        
AC_ARG_ENABLE([sctp], 
        AC_HELP_STRING([--disable-sctp],[disable sctp input plugin]))
AM_CONDITIONAL([HAVE_SCTP], [test "$enable_sctp" != "no"])

AC_ARG_ENABLE([doc],
        AC_HELP_STRING([--disable-doc],[disable documentation building]))
AM_CONDITIONAL([HAVE_DOC], [test "$enable_doc" != "no"])

# add --without-openssl parameter
AC_ARG_WITH([openssl],
	AC_HELP_STRING([--without-openssl],[disable TLS support]), [with_openssl=no], [with_openssl=yes])


############################ Check for programs ################################

# Check for flex
AC_CHECK_PROG(FLEX, flex, flex)
AS_IF([test -z "$FLEX"], [AC_MSG_ERROR([flex not found, please install flex package])])

# Check fol bison
AC_CHECK_PROG(BISON, bison, bison)
AS_IF([test -z "$BISON"], [AC_MSG_ERROR([bison not found, please install bison package])])

# Check for doxygen
AC_CHECK_PROG(DOXYGEN, doxygen, doxygen)
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])

# Check for rpmbuil
AC_CHECK_PROG(RPMBUILD, rpmbuild, rpmbuild)

# Check for xsltproc
LBR_CHECK_XSLTPROC

# Check for standard programs
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Check compiler visibility option
LBR_CHECK_VISIBILITY([hidden])
AS_IF([test "$VISIBILITY" = "no"], [AC_MSG_ERROR([compiler does not support -fvisibility])])

############################ Check for libraries ###############################

### LibXML2 ###
AC_CHECK_LIB([xml2], [main],
    [LIBS="`xml2-config --libs` $LIBS"
    CFLAGS="`xml2-config --cflags` $CFLAGS"],
    AC_MSG_ERROR([Libxml2 not found ]))


### TLS support ###
AS_IF([test "x$with_openssl" != xno],
  [AC_CHECK_LIB([ssl], [SSL_library_init],
    [TLS_SUPPORT="yes"
    # Check for pkg-config
	AC_CHECK_PROG([PKG_CONFIG], [pkg-config], ["yes"], ["no"])
	AS_IF([test "x$PKG_CONFIG" = xno], [AC_MSG_ERROR([pkg-config is required])])
	TLS_CPPFLAGS="-DTLS_SUPPORT"
	TLS_CFLAGS="`pkg-config --cflags libssl`"
	TLS_LIBS="`pkg-config --libs libssl`"],
    AC_MSG_ERROR([Required library openssl is missing])
  )], 
  [TLS_SUPPORT="no"])
AC_SUBST([TLS_CPPFLAGS])
AC_SUBST([TLS_CFLAGS])
AC_SUBST([TLS_LIBS])

### libsctp ###
# empty command on if-found-action is to prevent -lsctp to be added to LIBS
AM_COND_IF(HAVE_SCTP,
	[AC_CHECK_LIB([sctp], [sctp_bindx], [echo -n ""], 
		[AC_MSG_ERROR([sctp library not found, run with --disable-sctp to build without SCTP support or install libsctp package])]
	)]
)

### pthread ###
AC_CHECK_LIB([pthread], [pthread_create], 
        [CFLAGS="$CFLAGS -pthread"],
    	AC_MSG_ERROR([Required library pthread missing]))

### dynamic linker ###
AC_CHECK_LIB([dl], [dlopen],,
        AC_MSG_ERROR([Required library dl missing]))
        

######################### Checks for header files ##############################
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h syslog.h unistd.h])

AC_CHECK_HEADERS([libxml/xmlversion.h], , AC_MSG_ERROR([libxml headers missing. Please install libxml-devel package]), [AC_INCLUDES_DEFAULT])

AM_COND_IF(HAVE_SCTP,
	[AC_CHECK_HEADER([netinet/sctp.h], , 
		[AC_MSG_ERROR([netinet/sctp.h header not found, run with --disable-sctp to build without SCTP support])]
	)]
)

AS_IF([test "$with_openssl" != "no"],
	[AC_CHECK_HEADER([openssl/ssl.h], , 
		[AC_MSG_ERROR([openssl/ssl.h header not found])]
	)]
)

######## Checks for typedefs, structures, and compiler characteristics #########
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

######################## Checks for library functions ##########################
AC_CHECK_FUNCS([malloc])
AC_CHECK_FUNCS([realloc])
AC_FUNC_FORK
AC_CHECK_FUNCS([localtime_r memset pathconf select socket strchr strdup strerror strrchr strtol])
AC_CHECK_DECL([htobe64], [AC_DEFINE([HAVE_HTOBE64], [1],
                         [Define if macro htobe64 exists.])],,
			 [[#include <endian.h>]])
AC_CHECK_DECL([be64toh], [AC_DEFINE([HAVE_BE64TOH], [1],
                         [Define if macro be64toh exists.])],,
			 [[#include <endian.h>]])

# substitute automake CFLAGS and CPPFLAGS variables
AC_SUBST([AM_CPPFLAGS])        
AC_SUBST([AM_CFLAGS])

# generate output
AC_CONFIG_FILES([Makefile
				ipfixcol.spec
				ipfixcol.pc
				src/Makefile
				src/ipfixviewer/Makefile
				src/input/tcp/Makefile
				src/input/udp/Makefile
				src/input/ipfix/Makefile
				src/input/sctp/Makefile
				src/storage/ipfix/Makefile
				src/storage/dummy/Makefile
				src/storage/forwarding/Makefile
				src/intermediate/anonymization/Makefile
				src/intermediate/dummy/Makefile
				src/intermediate/joinflows/Makefile
				src/intermediate/filter/Makefile
				src/intermediate/odip/Makefile
				src/intermediate/hooks/Makefile
				src/utils/Makefile
				src/utils/ipfixconf/Makefile
				src/utils/ipfixsend/Makefile
				src/utils/libsiso/Makefile
				src/utils/profiles/Makefile
				config/Makefile
				headers/Makefile
				documentation/doxygen/Makefile
				documentation/man/Makefile])
AM_COND_IF(HAVE_DOXYGEN, AC_CONFIG_FILES([documentation/doxygen/doxyfile]))

AC_OUTPUT


######################## Checks for xsltproc and doxygen if doc building is enabled ##########################
AM_COND_IF(HAVE_DOC,
    [AM_COND_IF(HAVE_XSLTPROC, 
        [AM_COND_IF(HAVE_DOXYGEN, , AC_MSG_ERROR([Missing doxygen - install it or run with --disable-doc]))],
        AC_MSG_ERROR([Missing xsltproc - install it or run with --disable-doc])
    )]
)

AS_IF([test -z "$RPMBUILD"],
	AC_MSG_WARN([Due to missing rpmbuild you will not able to generate RPM package.])
)

# Print final summary
echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix........: $prefix
  Distribution..: $DISTRO
  Debug build...: ${enable_debug:-no}
  C Compiler....: $CC $AM_CFLAGS $CFLAGS
  C++ Compiler..: $CXX $AM_CXXFLAGS $CXXFLAGS $AM_CPPFLAGS
  Linker........: $LDFLAGS $LIBS
  rpmbuild......: ${RPMBUILD:-NONE}
  Build doc.....: ${enable_doc:-yes}
  xsltproc......: ${XSLTPROC:-NONE}
  xsltmanstyle..: $XSLTMANSTYLE
  flex..........: ${FLEX:-NONE}
  bison.........: ${BISON:-NONE}
  Doxygen.......: ${DOXYGEN:-NONE}
  TLS support...: $TLS_SUPPORT
  SCTP support..: ${enable_sctp:-yes}
"
