AC_DEFUN([SOLARIS_SOLOOKUP_TAKES_SOCKPARAMS], [
  AC_CACHE_CHECK([whether solookup takes a sockparams],
    [ac_cv_solaris_solookup_takes_sockparams],
    [AC_TRY_COMPILE(
	[#define _KERNEL
#include <sys/systm.h>
#include <sys/socketvar.h>],
	[struct sockparams *sp;
(void) solookup(AF_INET, SOCK_DGRAM, 0, &sp);], 
	[ac_cv_solaris_solookup_takes_sockparams=yes],
	[ac_cv_solaris_solookup_takes_sockparams=no])
  ])
  AS_IF([test "$ac_cv_solaris_solookup_takes_sockparams" = "yes"],
	[AC_DEFINE(SOLOOKUP_TAKES_SOCKPARAMS, 1,
		   [define if solookup takes a sockparams**])
  ])
])
