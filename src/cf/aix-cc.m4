dnl
dnl $Id$
dnl 

dnl We do not quite know how the version number of the xlc
dnl corresponds to the value of __IBMCC__ but 440 (decimal)
dnl seems to be the first compiler that works for us.
dnl 
dnl Guess:
dnl
dnl lslpp says			__IBMCC__
dnl
dnl vac.C 4.4.0.0		440
dnl vac.C 5.0.2.1		502
dnl

AC_DEFUN([AC_AIX_CC_GOOD], [
AIXCC="$CC"
save_CC="$CC"
if test "$CC" ; then
  AC_CHECK_PROGS(AIXCC, "$CC" xlc /usr/vac/bin/xlc gcc)
else
  AC_CHECK_PROGS(AIXCC, xlc /usr/vac/bin/xlc gcc)
fi
CC="$AIXCC"
AC_MSG_CHECKING(whether CC is a good enough Aix cc)
AC_CACHE_VAL(ac_cv_aix_cc_good,
[
XLCVERSION=440
AC_TRY_RUN(
[
int main(void) { return __IBMC__ < $XLCVERSION;}
], 
ac_cv_aix_cc_good=yes,
ac_cv_aix_cc_good=no,
ac_cv_aix_cc_good=no)
AC_MSG_RESULT($ac_cv_aix_cc_good)])
if test "$ac_cv_aix_cc_good" = "yes"; then
  AC_CHECK_PROGS(CC, "$AIXCC")
else
  CC="$save_CC"
fi
])

