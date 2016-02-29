AC_DEFUN([OPENAFS_TYPE_CHECKS],[
LIBS="$save_LIBS"

openafs_cv_saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $XCFLAGS_NOCHECKING"

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

OPENAFS_RETSIGTYPE
AC_CHECK_SIZEOF(void *)
AC_CHECK_SIZEOF(unsigned long long)
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(unsigned int)
AC_TYPE_INTPTR_T
AC_TYPE_UINTPTR_T
AC_TYPE_SSIZE_T
AC_CHECK_TYPE([sig_atomic_t],[],
    [AC_DEFINE([sig_atomic_t], [int],
        [Define to int if <signal.h> does not define.])],
[#include <sys/types.h>
#include <signal.h>])
AC_CHECK_TYPE([socklen_t],[],
    [AC_DEFINE([socklen_t], [int],
        [Define to int if <sys/socket.h> does not define.])],
[#include <sys/types.h>
#include <sys/socket.h>])
AC_CHECK_TYPES(off64_t)
AC_CHECK_TYPES([ssize_t], [], [], [#include <unistd.h>])
AC_CHECK_TYPES([struct winsize], [], [], [
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# include <sys/termios.h>
#endif
#include <sys/ioctl.h>])
AC_CHECK_TYPES([sa_family_t, socklen_t, struct sockaddr,
                struct sockaddr_storage],
               [], [], [
#include <sys/types.h>
#include <sys/socket.h>
])
AC_CHECK_TYPES([sa_family_t], [], [], [
#include <sys/types.h>
#include <sys/socket.h>
])
AC_CHECK_TYPES([struct addrinfo], [], [], [
#include <sys/types.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
])
AC_CHECK_TYPES([long long], [], [], [])

AC_CHECK_SIZEOF([long])

CFLAGS="$openafs_cv_saved_CFLAGS"
])
