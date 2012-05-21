AC_PREREQ([2.60])
AC_INIT([ipfixcol-projects], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
LT_PREREQ([2.2])
LT_INIT([dlopen])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SUBDIRS([ base 
                    tools/fbitdump
                    tools/fbitconvert
                    plugins/storage/fastbit
                    plugins/storage/postgres])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
