dnl FUSE Autoconf glue.  Build with FUSE if it's available; if it's not,
dnl don't enable the FUSE build.  If --enable is given explicitly, FUSE
dnl must be found or we bail out.

AC_DEFUN([OPENAFS_FUSE],
[openafs_fuse=
 ENABLE_FUSE_CLIENT=
 CLIENT_UAFS_DEP=
 AC_ARG_ENABLE([fuse-client],
    [AS_HELP_STRING([--disable-fuse-client],
        [disable building of the FUSE userspace client, afsd.fuse
         (defaults to enabled)])],
    [openafs_fuse="$enableval"])

 AS_IF([test -z "$openafs_fuse"],
    [PKG_CHECK_EXISTS([fuse3], [openafs_fuse=yes])])

 AS_IF([test x"$openafs_fuse" = xyes],
    [PKG_CHECK_MODULES([FUSE], [fuse3])
     ENABLE_FUSE_CLIENT=afsd.fuse
     CLIENT_UAFS_DEP=libuafs])
 AC_SUBST([ENABLE_FUSE_CLIENT])
 AC_SUBST([CLIENT_UAFS_DEP])
 AC_SUBST([FUSE_CFLAGS])
 AC_SUBST([FUSE_LIBS])])
