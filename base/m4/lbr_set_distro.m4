# LBR_SET_DISTRO(["distro"])
# --------------------------
# LBR_SET_DISTRO tries to determine current linux distribution.
# It uses AC_ARG_WITH to enable the user to specify the distribution.
# It sets and substitutes variable DISTRO.
#
# If no arguments are given and macro is unable to determine
# the distribution, the "redhat" distro is assumed. If the "distro"
# argument is passed, it is used as the default distribution.
# The user option always superseeds other settings.
#
# Currently the macro recognizes following distributions:
# redhat
# debian
# mandrake
# suse
#
# Author: Petr Velan <petr.velan@cesnet.cz>
# Modified: 2012-05-03
#
AC_DEFUN([LBR_SET_DISTRO],
[m4_ifval([$1],[DISTRO=$1],[DISTRO="redhat"])
# Autodetect current distribution
if test -f /etc/redhat-release; then
	DISTRO=redhat
elif test -f /etc/SuSE-release; then
	DISTRO=suse
elif test -f /etc/mandrake-release; then
	DISTRO='mandrake'
elif test -f /etc/debian_version; then
	DISTRO=debian
fi
# Check if distribution was specified manually
AC_ARG_WITH([distro],
	AC_HELP_STRING([--with-distro=DISTRO],[Compile for specific Linux distribution]),
	DISTRO=$withval,
	AC_MSG_NOTICE([Detected distribution: $DISTRO. Run with --with-distro=DISTRO to override]))
AC_SUBST(DISTRO)
])# LBR_SET_DISTRO
