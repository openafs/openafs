dnl SWIG Autoconf glue. Build with SWIG-derived language bindings if SWIG
dnl is available; if it's not, don't build the extra language bindings.

AC_DEFUN([OPENAFS_SWIG],
[AC_CHECK_PROG([SWIG], [swig], [swig])
LIBUAFS_BUILD_PERL=
if test "x$SWIG" = "xswig" ; then
	LIBUAFS_BUILD_PERL=LIBUAFS_BUILD_PERL
fi
AC_SUBST(LIBUAFS_BUILD_PERL)
])
