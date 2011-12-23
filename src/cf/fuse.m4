dnl FUSE Autoconf glue.  Build with FUSE if it's available; if it's not,
dnl don't enable the FUSE build.  If --enable is given explicitly, FUSE
dnl must be found or we bail out.

AC_DEFUN([OPENAFS_FUSE_DEFS],
 [ENABLE_FUSE_CLIENT=afsd.fuse
  CLIENT_UAFS_DEP=libuafs])

dnl Solaris 11 has a FUSE package, but it does not come with a pkg-config
dnl fuse.pc configuration. The libraries, headers, etc are in predictable
dnl places, though.
AC_DEFUN([OPENAFS_SUN511_FUSE],
 [AS_CASE([$AFS_SYSNAME],
   [sun4x_511|sunx86_511],
    [sol11fuse=
     fuse_cppflags=-D_FILE_OFFSET_BITS=64
     save_CPPFLAGS="$CPPFLAGS"
     CPPFLAGS="$fuse_cppflags $CPPFLAGS"
     AC_CHECK_HEADER([fuse.h], [sol11fuse=yes])
     AS_IF([test x"$sol11fuse" = xyes],
      [FUSE_CFLAGS="$fuse_cppflags"
       FUSE_LIBS=-lfuse
       openafs_fuse=yes
       OPENAFS_FUSE_DEFS])
     CPPFLAGS="$save_CPPFLAGS"])])

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
    [PKG_CHECK_EXISTS([fuse], [openafs_fuse=yes], [OPENAFS_SUN511_FUSE])])

 AS_IF([test x"$openafs_fuse" = xyes && test x"$ENABLE_FUSE_CLIENT" = x],
    [PKG_CHECK_MODULES([FUSE], [fuse],
       [OPENAFS_FUSE_DEFS],
       [OPENAFS_SUN511_FUSE
           AS_IF([test x"ENABLE_FUSE_CLIENT" = x],
               [AC_MSG_ERROR(["$FUSE_PACKAGE_ERRORS"])])])])
 AC_SUBST([ENABLE_FUSE_CLIENT])
 AC_SUBST([CLIENT_UAFS_DEP])
 AC_SUBST([FUSE_CFLAGS])
 AC_SUBST([FUSE_LIBS])])
