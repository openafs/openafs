AC_DEFUN([OPENAFS_SOCKET_CHECKS],[
AC_CACHE_CHECK([if struct sockaddr has sa_len field],
    [ac_cv_sockaddr_len],
    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <sys/socket.h>]], [[struct sockaddr *a; a->sa_len=0;]])],[ac_cv_sockaddr_len=yes],[ac_cv_sockaddr_len=no])
])
AS_IF([test "$ac_cv_sockaddr_len" = "yes"],
      [AC_DEFINE(STRUCT_SOCKADDR_HAS_SA_LEN, 1,
                 [define if you struct sockaddr sa_len])])
])

AC_DEFUN([OPENAFS_SOCKOPT_CHECK],[
AC_CACHE_CHECK([for setsockopt(, SOL_IP, IP_RECVERR)],
    [ac_cv_setsockopt_iprecverr],
    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>]], [[int on=1;
setsockopt(0, SOL_IP, IP_RECVERR, &on, sizeof(on));]])],[ac_cv_setsockopt_iprecverr=yes],[ac_cv_setsockopt_iprecverr=no])])

AS_IF([test "$ac_cv_setsockopt_iprecverr" = "yes"],
      [AC_DEFINE([HAVE_SETSOCKOPT_IP_RECVERR], [1],
                 [define if we can receive socket errors via IP_RECVERR])])
])
