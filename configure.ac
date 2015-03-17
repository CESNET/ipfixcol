AC_PREREQ([2.60])
AC_INIT([ipfixcol-projects], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])
LT_PREREQ([2.2])
LT_INIT([dlopen])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SUBDIRS([ base
    tools/fbitdump
    tools/fbitconvert
    tools/fbitmerge
    tools/fbitexpire
    plugins/storage/fastbit
    plugins/storage/fastbit_compression
    plugins/storage/postgres
    plugins/storage/unirec
    plugins/input/nfdump
	plugins/intermediate/geoip
	plugins/intermediate/profiler
	plugins/intermediate/profile_stats
	plugins/intermediate/stats
    plugins/storage/json])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
