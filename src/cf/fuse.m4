dnl
dnl $Id$
dnl
dnl FUSE autoconf glue
dnl

AC_DEFUN([OPENAFS_FUSE],[

AC_ARG_ENABLE([fuse-client],
    [AS_HELP_STRING([--enable-fuse-client],[enable building of the FUSE userspace client, afsd.fuse])],,
    [enable_fuse_client="no"])

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
