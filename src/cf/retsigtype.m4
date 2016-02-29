dnl
dnl $Id$
dnl
dnl Figure out return type of signal handlers, and define SIGRETURN macro
dnl that can be used to return from one
dnl
AC_DEFUN([OPENAFS_RETSIGTYPE],[
AC_DIAGNOSE([obsolete],[your code may safely assume C89 semantics that RETSIGTYPE is void.
Remove this warning and the `AC_CACHE_CHECK' when you adjust the code.])dnl
AC_CACHE_CHECK([return type of signal handlers],[ac_cv_type_signal],[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([#include <sys/types.h>
#include <signal.h>
],
		 [return *(signal (0, 0)) (0) == 1;])],
		   [ac_cv_type_signal=int],
		   [ac_cv_type_signal=void])])
AC_DEFINE_UNQUOTED([RETSIGTYPE],[$ac_cv_type_signal],[Define as the return type of signal handlers
		    (`int' or `void').])

if test "$ac_cv_type_signal" = "void" ; then
	AC_DEFINE(VOID_RETSIGTYPE, 1, [Define if signal handlers return void.])
fi

AH_BOTTOM([#ifdef VOID_RETSIGTYPE
#define SIGRETURN(x) return
#else
#define SIGRETURN(x) return (RETSIGTYPE)(x)
#endif])
])
