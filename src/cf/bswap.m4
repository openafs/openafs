AC_DEFUN([OPENAFS_BSWAP_CHECKS],[
dnl Stuff that's harder ...
AC_MSG_CHECKING([for bswap16])
AC_LINK_IFELSE([AC_LANG_PROGRAM([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif
],
[short a, b; b = bswap16(a); ])],
[AC_MSG_RESULT(yes)
 AC_DEFINE(HAVE_BSWAP16, 1, [Define to 1 if you have the bswap16 function])
],
[AC_MSG_RESULT(no)])

AC_MSG_CHECKING([for bswap32])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif
],
[int a, b; b = bswap32(a); ])],
[AC_MSG_RESULT(yes)
 AC_DEFINE(HAVE_BSWAP32, 1, [Define to 1 if you have the bswap32 function])
],
[AC_MSG_RESULT(no)])
])
