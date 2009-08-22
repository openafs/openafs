dnl
dnl $Id$
dnl 

dnl
dnl The Solaris compiler defines __SUNPRO_C as a hex number
dnl whose digits correspond to the digits in the compiler
dnl version. Thus compiler version 4.2.0 is 0x420.
dnl

AC_DEFUN([AC_SOLARIS_CC_GOOD], [
SOLARISCC="$CC"
save_CC="$CC"
if test "$CC" ; then
  AC_CHECK_PROGS(SOLARISCC, "$CC" cc /opt/SUNWspro/bin/cc gcc)
else
  AC_CHECK_PROGS(SOLARISCC, cc /opt/SUNWspro/bin/cc gcc)
fi
CC="$SOLARISCC"
AC_MSG_CHECKING(whether CC is a good enough Solaris cc)
AC_CACHE_VAL(ac_cv_solaris_cc_good,
[
SUNPROVERSION=0x400
AC_TRY_RUN(
[
int main(void) { return __SUNPRO_C < $SUNPROVERSION;}
], 
ac_cv_solaris_cc_good=yes,
ac_cv_solaris_cc_good=no,
ac_cv_solaris_cc_good=no)
AC_MSG_RESULT($ac_cv_solaris_cc_good)])
if test "$ac_cv_solaris_cc_good" = "yes"; then
  AC_CHECK_PROGS(CC, "$SOLARISCC")
else
  CC="$save_CC"
fi
])

