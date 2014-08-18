# LBR_SET_CXXSTD()
# --------------------------
# LBR_SET_CXXSTD tries to determine lastest usable standard for compiler.
# It checks following standards: 
# gnu++11
# gnu++03
# gnu++98
# The flag for found standard is set in CXXSTD variable.
#
# The macro takes no arguments.
#
# Author: Petr Velan <petr.velan@cesnet.cz>
# Modified: 2013-01-04
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
AS_IF([ test -z "$CXXSTD" ],
	[CXXFLAGS=-std=gnu++03
	AC_MSG_CHECKING([whether CC supports -std=gnu++03])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
	    [AC_MSG_RESULT([yes])]
	    [CXXSTD="$CXXFLAGS"],
		[AC_MSG_RESULT([no])]
	)]
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
CXXFLAGS="$my_save_cflags"
])# LBR_SET_CXXSTD
