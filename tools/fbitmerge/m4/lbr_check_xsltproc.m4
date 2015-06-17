# LBR_CHECK_XSLTPROC()
# ----------------------------------
# LBR_CHECK_XSLTPROC checks for xsltproc program and substitutes
# XSLTPROC variable with found program.
#
# Sets HAVE_XSLTPROC automake conditional variable.
#
# Variables XSLTHTMLSTYLE, XSLTXHTMLSTYLE, XSLTMANSTYLE
# and MANHTMLCSS are substituted with paths of xsd styles.
#
# The macro needs the LBR_SET_DISTRO to be called first, since
# it xsd styles are in different paths depending on distribution.
#
# Currently the macro knows the location of styles in following 
# distributions:
#
# redhat
# suse
# mandrake
# debian
# arch
#
# Author: Petr Velan <petr.velan@cesnet.cz>
# Modified: 2015-06-12
#
AC_DEFUN([LBR_CHECK_XSLTPROC],
[AC_REQUIRE([LBR_SET_DISTRO])dnl
# Check for xsltproc
AC_CHECK_PROG(XSLTPROC, xsltproc, xsltproc)
AM_CONDITIONAL([HAVE_XSLTPROC], [test -n "$XSLTPROC"])
dnl
# Check for Docbook stylesheets for manpages
if test -n "$XSLTPROC"; then
    case $DISTRO in
        redhat )
            if test -f /usr/share/sgml/docbook/xsl-stylesheets/manpages/docbook.xsl; then
                XSLTMANSTYLE="/usr/share/sgml/docbook/xsl-stylesheets/manpages/docbook.xsl"
                XSLTHTMLSTYLE="/usr/share/sgml/docbook/xsl-stylesheets/html/docbook.xsl"
                XSLTXHTMLSTYLE="/usr/share/sgml/docbook/xsl-stylesheets/xhtml/docbook.xsl"
                BUILDREQS="$BUILDREQS docbook-style-xsl"
            else
                AC_MSG_ERROR(["Docbook XSL stylesheet for man pages not found!"])
            fi
            ;;
        suse )
            if test -f /usr/share/xml/docbook/stylesheet/nwalsh5/current/manpages/docbook.xsl; then
                XSLTMANSTYLE="/usr/share/xml/docbook/stylesheet/nwalsh5/current/manpages/docbook.xsl"
                XSLTHTMLSTYLE="/usr/share/xml/docbook/stylesheet/nwalsh5/current/html/docbook.xsl"
                XSLTXHTMLSTYLE="/usr/share/xml/docbook/stylesheet/nwalsh5/current/xhtml/docbook.xsl"
                BUILDREQS="$BUILDREQS docbook5-xsl-stylesheets"
            elif test -f /usr/share/xml/docbook/stylesheet/nwalsh/current/manpages/docbook.xsl; then
                XSLTMANSTYLE="/usr/share/xml/docbook/stylesheet/nwalsh/current/manpages/docbook.xsl"
                XSLTHTMLSTYLE="/usr/share/xml/docbook/stylesheet/nwalsh/current/html/docbook.xsl"
                XSLXTMLSTYLE="/usr/share/xml/docbook/stylesheet/nwalsh/current/xhtml/docbook.xsl"
                BUILDREQS="$BUILDREQS docbook-xsl-stylesheets"
            else
                AC_MSG_ERROR(["Docbook XSL stylesheet for man pages not found!"])
            fi
            ;;
        debian )
            if test -f /usr/share/xml/docbook/stylesheet/docbook-xsl/manpages/docbook.xsl; then
                XSLTMANSTYLE="/usr/share/xml/docbook/stylesheet/docbook-xsl/manpages/docbook.xsl"
                XSLTHTMLSTYLE="/usr/share/xml/docbook/stylesheet/docbook-xsl/html/docbook.xsl"
                XSLTXHTMLSTYLE="/usr/share/xml/docbook/stylesheet/docbook-xsl/xhtml/docbook.xsl"
            else
                AC_MSG_ERROR(["Docbook XSL stylesheet for man pages not found!"])
            fi
            ;;
        arch )
            ARCH_DOCBOOK_VERSION=$(pacman -Q docbook-xsl | cut -d ' ' -f 2 | cut -d '-' -f 1)
            if test -f /usr/share/xml/docbook/xsl-stylesheets-$ARCH_DOCBOOK_VERSION/manpages/docbook.xsl; then
                XSLTMANSTYLE="/usr/share/xml/docbook/xsl-stylesheets-$ARCH_DOCBOOK_VERSION/manpages/docbook.xsl"
                XSLTHTMLSTYLE="/usr/share/xml/docbook/xsl-stylesheets-$ARCH_DOCBOOK_VERSION/html/docbook.xsl"
                XSLTXHTMLSTYLE="/usr/share/xml/docbook/xsl-stylesheets-$ARCH_DOCBOOK_VERSION/xhtml/docbook.xsl"
            else
                AC_MSG_ERROR(["Docbook XSL stylesheet for man pages not found!"])
            fi
            ;;
        * )
            AC_MSG_ERROR([Unsupported Linux distribution])
            ;;
    esac

    # and path to CSS for HTML
    # TODO: find some usefull style and use it here
    #MANHTMLCSS="--stringparam html.stylesheet http://linuxmanpages.com/global/main.css"
fi
AC_SUBST(XSLTHTMLSTYLE)
AC_SUBST(XSLTXHTMLSTYLE)
AC_SUBST(XSLTMANSTYLE)
AC_SUBST(MANHTMLCSS)
])# LBR_CHECK_XSLTPROC
