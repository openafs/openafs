AC_DEFUN([SOLARIS_PROC_HAS_P_COREFILE], [
AC_MSG_CHECKING(for p_corefile in struct proc)
AC_CACHE_VAL(ac_cv_solaris_proc_has_p_corefile,
[
AC_TRY_COMPILE(
[#define _KERNEL
#include <sys/proc.h>],
[struct proc _proc;
(void) _proc.p_corefile;], 
ac_cv_solaris_proc_has_p_corefile=yes,
ac_cv_solaris_proc_has_p_corefile=no)])
AC_MSG_RESULT($ac_cv_solaris_proc_has_p_corefile)
if test "$ac_cv_solaris_proc_has_p_corefile" = "yes"; then
  AC_DEFINE([HAVE_P_COREFILE], 1, [define if struct proc has p_corefile])
fi
])

