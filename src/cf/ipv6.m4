dnl IPv6 support detection for OpenAFS.
dnl
dnl Defines HAVE_IPV6 when the platform provides the types and constants that
dnl src/rx/rx_addr.c needs in order to represent an IPv6 address.  Passing
dnl --without-ipv6 forces it off; passing --with-ipv6 makes a platform without
dnl IPv6 support a hard configure error rather than a silent downgrade.
dnl
dnl The individual socket options are probed separately, because a platform can
dnl have the types without having all of the options (and because rx needs to
dnl know which of them it may use).

AC_DEFUN([OPENAFS_IPV6_CHECKS],[

AC_ARG_WITH([ipv6],
    [AS_HELP_STRING([--without-ipv6],
        [disable IPv6 support (default: enabled where the platform allows)])],
    [],
    [with_ipv6=maybe])

openafs_ipv6=no
AS_IF([test "x$with_ipv6" != "xno"],
    [AC_CACHE_CHECK([for IPv6 support],
        [openafs_cv_ipv6],
        [AC_COMPILE_IFELSE(
            [AC_LANG_PROGRAM(
                [[#include <sys/types.h>
                  #include <sys/socket.h>
                  #include <netinet/in.h>]],
                [[struct sockaddr_in6 sin6;
                  struct in6_addr in6;
                  sin6.sin6_family = AF_INET6;
                  sin6.sin6_port = 0;
                  in6 = sin6.sin6_addr;
                  sin6.sin6_addr = in6;
                  return sin6.sin6_family;]])],
            [openafs_cv_ipv6=yes],
            [openafs_cv_ipv6=no])])
     openafs_ipv6=$openafs_cv_ipv6])

AS_IF([test "x$with_ipv6" = "xyes" && test "x$openafs_ipv6" != "xyes"],
      [AC_MSG_ERROR([--with-ipv6 was given, but this platform lacks IPv6 support])])

AS_IF([test "x$openafs_ipv6" = "xyes"],
      [AC_DEFINE([HAVE_IPV6], [1],
                 [define if the platform supports IPv6 addressing])])

dnl The remaining probes are only meaningful when we have IPv6 at all.
AS_IF([test "x$openafs_ipv6" = "xyes"],[

    AC_CACHE_CHECK([if struct sockaddr_in6 has sin6_len field],
        [openafs_cv_sockaddr_in6_len],
        [AC_COMPILE_IFELSE(
          [AC_LANG_PROGRAM(
            [[#include <sys/types.h>
              #include <sys/socket.h>
              #include <netinet/in.h>]],
            [[struct sockaddr_in6 *a; a->sin6_len=0;]])],
          [openafs_cv_sockaddr_in6_len=yes],
          [openafs_cv_sockaddr_in6_len=no])])
    AS_IF([test "$openafs_cv_sockaddr_in6_len" = "yes"],
          [AC_DEFINE([STRUCT_SOCKADDR_HAS_SA6_LEN], [1],
                     [define if struct sockaddr_in6 has a sin6_len field])])

    OPENAFS_IPV6_SOCKOPT([IPV6_V6ONLY], [HAVE_IPV6_V6ONLY],
        [define if the IPV6_V6ONLY socket option is available])
    OPENAFS_IPV6_SOCKOPT([IPV6_RECVERR], [HAVE_IPV6_RECVERR],
        [define if the IPV6_RECVERR socket option is available])
    OPENAFS_IPV6_SOCKOPT([IPV6_MTU_DISCOVER], [HAVE_IPV6_MTU_DISCOVER],
        [define if the IPV6_MTU_DISCOVER socket option is available])
])

dnl Enumerating IPv6 interfaces needs getifaddrs(); SIOCGIFCONF cannot report
dnl them, because struct ifreq is too small to hold an IPv6 address.
AC_CHECK_HEADERS([ifaddrs.h])
AC_CHECK_FUNCS([getifaddrs])
])

dnl OPENAFS_IPV6_SOCKOPT(CONSTANT, DEFINE, DESCRIPTION)
dnl
dnl Probe for an IPv6 socket option constant.  We only compile-test the
dnl constant rather than calling setsockopt(), since the call would fail at
dnl link time on nothing and succeed at compile time everywhere.
AC_DEFUN([OPENAFS_IPV6_SOCKOPT],[
AC_CACHE_CHECK([for $1],
    [AS_TR_SH([openafs_cv_have_$1])],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
        [[#include <sys/types.h>
          #include <sys/socket.h>
          #include <netinet/in.h>]],
        [[int opt = $1; return opt;]])],
      [AS_TR_SH([openafs_cv_have_$1])=yes],
      [AS_TR_SH([openafs_cv_have_$1])=no])])
AS_IF([test "x$AS_TR_SH([openafs_cv_have_$1])" = "xyes"],
      [AC_DEFINE([$2], [1], [$3])])
])
