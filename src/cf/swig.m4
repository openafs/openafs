dnl SWIG Autoconf glue. Build with SWIG-derived language bindings if SWIG
dnl is available; if it's not, don't build the extra language bindings.
dnl
dnl Specify --without-swig (or --with-swig=no) to override the SWIG check and
dnl disable features which require SWIG. Specify --with-swig=yes to force the
dnl SWIG check and fail if not detected.
dnl
AC_DEFUN([OPENAFS_SWIG],

[AC_ARG_WITH([swig],
  [AS_HELP_STRING([--with-swig],
    [use swig to generate language bindings (defaults to autodetect)])],
  [],
  [with_swig=check])

LIBUAFS_BUILD_PERL=
AS_IF([test "x$with_swig" != "xno"],
  [AC_CHECK_PROG([SWIG], [swig], [swig])
  AS_IF([test "x$SWIG" = "xswig"],
    [LIBUAFS_BUILD_PERL=LIBUAFS_BUILD_PERL])
  AS_IF([test "x$with_swig" = "xyes"],
    [AS_IF([test "x$SWIG" != "xswig"],
      [AC_MSG_ERROR([swig requested but not found])])])])

AC_SUBST(LIBUAFS_BUILD_PERL)
])
