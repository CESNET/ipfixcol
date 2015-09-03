# LBR_SET_CXXSTD([ENSURE_STD])
# --------------------------
# LBR_SET_CXXSTD tries to determine lastest usable standard for compiler.
# It checks following standards: 
# gnu++11
# gnu++0x
# gnu++03
# gnu++98
# The flag for found standard is set in CXXSTD variable.
#
# The macro takes an optional ENSURE_STD argument. It must be set to one of
# the supported standards. The macro fails if the standard is not supported
#
# Author: Petr Velan <petr.velan@cesnet.cz>
# Modified: 2015-08-04
#
AC_DEFUN([LBR_SET_CXXSTD],
[
my_save_cxxflags="$CXXFLAGS"
CXXFLAGS=-std=gnu++11
AC_MSG_CHECKING([whether CC supports -std=gnu++11])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
    [AC_MSG_RESULT([yes])]
    [CXXSTD="$CXXFLAGS"],
    [AC_MSG_RESULT([no])]
)
AS_IF([ test "-std=$1" = "$CXXFLAGS" -a "$CXXSTD" != "$CXXFLAGS" ],
	AC_MSG_ERROR([C++ compiler does not support $1 ])
)
AS_IF([ test -z "$CXXSTD" ],
	[CXXFLAGS=-std=gnu++0x
	AC_MSG_CHECKING([whether CC supports -std=gnu++0x])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
	    [AC_MSG_RESULT([yes])]
	    [CXXSTD="$CXXFLAGS"],
		[AC_MSG_RESULT([no])]
	)]
)
AS_IF([ test "-std=$1" = "$CXXFLAGS" -a "$CXXSTD" != "$CXXFLAGS" ],
	AC_MSG_ERROR([C++ compiler does not support $1 ])
)
AS_IF([ test -z "$CXXSTD" ],
	[CXXFLAGS=-std=gnu++03
	AC_MSG_CHECKING([whether CC supports -std=gnu++03])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
	    [AC_MSG_RESULT([yes])]
	    [CXXSTD="$CXXFLAGS"],
		[AC_MSG_RESULT([no])]
	)]
)
AS_IF([ test "-std=$1" = "$CXXFLAGS" -a "$CXXSTD" != "$CXXFLAGS" ],
	AC_MSG_ERROR([C++ compiler does not support $1 ])
)
AS_IF([ test -z "$CXXSTD" ],
	[CXXFLAGS=-std=gnu++98
	AC_MSG_CHECKING([whether CC supports -std=gnu++98])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
		[AC_MSG_RESULT([yes])]
	    [CXXSTD="$CXXFLAGS"],
	    [AC_MSG_RESULT([no])]
	)]
)
AS_IF([ test "-std=$1" = "$CXXFLAGS" -a "$CXXSTD" != "$CXXFLAGS" ],
	AC_MSG_ERROR([C++ compiler does not support $1 ])
)
CXXFLAGS="$my_save_cflags"
])# LBR_SET_CXXSTD
