# LBR_SET_CREDENTIALS()
# -----------------------------------------------
# LBR_SET_CREDENTIALS sets substitutes variables 
# USERNAME and USERMAIL to values retreived from git config. 
#
# Author: Petr Velan <petr.velan@cesnet.cz>
# Modified: 2012-05-05
#
AC_DEFUN([LBR_SET_CREDENTIALS],
[USERNAME=`git config --get user.name`
USERMAIL=`git config --get user.email`
AC_SUBST(USERNAME)
AC_SUBST(USERMAIL)
AC_MSG_NOTICE([Using username "$USERNAME" and email "$USERMAIL"])
])# LBR_SET_CREDENTIALS 
