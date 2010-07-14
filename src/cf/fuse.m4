dnl
dnl $Id$
dnl
dnl FUSE autoconf glue
dnl

AC_DEFUN([OPENAFS_FUSE],[

AC_ARG_ENABLE([fuse-client],
    [AS_HELP_STRING([--disable-fuse-client],[disable building of the FUSE userspace client, afsd.fuse (defaults to enabled)])],,
    [enable_fuse_client="yes"])

if test "x$enable_fuse_client" = "xyes" ; then
   PKG_PROG_PKG_CONFIG
   PKG_CHECK_MODULES([FUSE], [fuse])
   ENABLE_FUSE_CLIENT=afsd.fuse
   CLIENT_UAFS_DEP=libuafs
fi

AC_SUBST(ENABLE_FUSE_CLIENT)
AC_SUBST(CLIENT_UAFS_DEP)
AC_SUBST(FUSE_CFLAGS)
AC_SUBST(FUSE_LIBS)

])dnl
