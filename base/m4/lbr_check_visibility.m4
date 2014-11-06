# LBR_CHECK_VISIBILITY(["visibility"])
# --------------------------
# LBR_CHECK_VISIBILITY determines whether the compiler supports
# -fvisibility option.
#
# Default visibility OPTION to try is "hidden". It can be 
# changed by passing a parameter to the macro.
#
# Variable VISIBILITY is set to "yes" and CLAGS are extended by 
# -fvisibility=OPTION if the option is available,
# otherwise the CFLAGS are left untouched and 
# VISIBILITY is set to "no".
#
# Author: Petr Velan <petr.velan@cesnet.cz>
# Modified: 2014-06-11
#
AC_DEFUN([LBR_CHECK_VISIBILITY],
[m4_ifval([$1],[OPTION=$1],[OPTION="hidden"])
# Check for compiler's visibility attribute
AC_MSG_WARN([beware of $OPTION])
SAVE_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=$OPTION"
AC_MSG_CHECKING(for ELF visibility)
AC_COMPILE_IFELSE([
	AC_LANG_PROGRAM([[
		#pragma GCC visibility push($OPTION)
		__attribute__((visibility("default")))
		int var=10;
		]])
	],[
	AC_MSG_RESULT([yes])
	API="__attribute__((visibility(\"default\")))"
	VISIBILITY="yes"
	],[
	AC_MSG_RESULT([no])
	CFLAGS="$SAVE_CFLAGS"
	VISIBILITY="no"
	]
)
AC_SUBST(VISIBILITY)
])# LBR_CHECK_VISIBILITY
