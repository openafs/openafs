dnl Copyright (c) 2007
dnl The Regents of the University of Michigan
dnl ALL RIGHTS RESERVED
dnl
dnl Permission is granted to use, copy, create derivative works
dnl and redistribute this software and such derivative works
dnl for any purpose, so long as the name of the University of
dnl Michigan is not used in any advertising or publicity
dnl pertaining to the use or distribution of this software
dnl without specific, written prior authorization.  If the
dnl above copyright notice or any other identification of the
dnl University of Michigan is included in any copy of any
dnl portion of this software, then the disclaimer below must
dnl also be included.
dnl
dnl This software is provided as is, without representation
dnl from the University of Michigan as to its fitness for any
dnl purpose, and without warranty by the University of
dnl Michigan of any kind, either express or implied, including
dnl without limitation the implied warranties of
dnl merchantability and fitness for a particular purpose.  The
dnl regents of the University of Michigan shall not be liable
dnl for any damages, including special, indirect, incidental, or
dnl consequential damages, with respect to any claim arising
dnl out of or in connection with the use of the software, even
dnl if it has been or is hereafter advised of the possibility of
dnl such damages.
dnl
AC_DEFUN([AC_DARWIN_EXP_DC], [#
# Current MacOS kerberos libaries do not export all the
# functionality required by rxk5.  Worse yet, it implements
# its own unique internal credentials cache and does not
# provide a standalone external api to access that cache.
# Shame, shame, shame.
#
# The simple solution is to use file based credentials caches.
# You should go use that, and not read any further.
#
# This hack enables use of code that hooks up to one internal
# mechanism used by one version of kerberos (65-10).  Success with
# any other version is unlikely.  Use with any version is unwise.
# EXPERIMENTAL USE ONLY.  You were warned.
#
AC_ARG_ENABLE([temp-macosx-kludge],
[  --enable-temp-macosx-kludge   experimenal use only; do not use],,enable_temp_macosx_kludge=no)
m4_divert_text([DEFAULTS], [ENABLE_DC='#'])dnl
if test X"$enable_temp_macosx_kludge" == Xyes; then
	ENABLE_DC=''
fi
AC_SUBST(ENABLE_DC)])
