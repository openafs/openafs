AC_DEFUN([SOLARIS_SOLOOKUP_TAKES_SOCKPARAMS], [
AC_MSG_CHECKING(whether solookup takes a sockparams)
AC_CACHE_VAL(ac_cv_solaris_solookup_takes_sockparams,
[
AC_TRY_COMPILE(
[#define _KERNEL
#include <sys/systm.h>
#include <sys/socketvar.h>],
[struct sockparams *sp;
(void) solookup(AF_INET, SOCK_DGRAM, 0, &sp);], 
ac_cv_solaris_solookup_takes_sockparams=yes,
ac_cv_solaris_solookup_takes_sockparams=no)])
AC_MSG_RESULT($ac_cv_solaris_solookup_takes_sockparams)
if test "$ac_cv_solaris_solookup_takes_sockparams" = "yes"; then
  AC_DEFINE(SOLOOKUP_TAKES_SOCKPARAMS, 1, [define if solookup takes a sockparams**])
fi
])
