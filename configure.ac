AC_PREREQ([2.60])
AC_INIT([ipfixcol-projects], [1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability tar-ustar])
LT_PREREQ([2.2])
LT_INIT([dlopen])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SUBDIRS([ base
	tools/fbitdump
	tools/fbitconvert
	tools/fbitmerge
	tools/fbitexpire
	tools/profilesdaemon
	plugins/storage/fastbit
	plugins/storage/fastbit_compression
	plugins/storage/postgres
	plugins/storage/json
	plugins/storage/statistics
	plugins/storage/nfdump
	plugins/input/nfdump
	plugins/intermediate/geoip
	plugins/intermediate/profiler
	plugins/intermediate/profile_stats
	plugins/intermediate/stats
	plugins/intermediate/uid])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

# plugins/storage/unirec is not added to SUBDIRS due to its dependency on libtrap
