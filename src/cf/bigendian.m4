dnl
dnl $Id$
dnl

dnl check if this computer is little or big-endian
dnl if we can figure it out at compile-time then don't define the cpp symbol
dnl otherwise test for it and define it.  also allow options for overriding
dnl it when cross-compiling

AC_DEFUN([OPENAFS_CHECK_BIGENDIAN], [
AC_ARG_ENABLE([bigendian],
    [AS_HELP_STRING([--enable-bigendian], [the target is big endian])],
    [ac_cv_c_bigendian=yes])
AC_ARG_ENABLE([littleendian],
    [AS_HELP_STRING([--enable-littleendian], [the target is little endian])],
    [ac_cv_c_bigendian=no])

AC_CACHE_CHECK(whether byte order is known at compile time,
openafs_cv_c_bigendian_compile,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/param.h>]], [[
#if !BYTE_ORDER || !BIG_ENDIAN || !LITTLE_ENDIAN
 bogus endian macros
#endif]])],[openafs_cv_c_bigendian_compile=yes],[openafs_cv_c_bigendian_compile=no])])

AC_C_BIGENDIAN(
 [ac_cv_c_bigendian=yes],
 [ac_cv_c_bigendian=no],
 [AC_MSG_ERROR([specify either --enable-bigendian or --enable-littleendian])])

if test "$ac_cv_c_bigendian" = "yes"; then
  AC_DEFINE(AUTOCONF_FOUND_BIGENDIAN, 1, [define if target is big endian])dnl
fi
if test "$openafs_cv_c_bigendian_compile" = "yes"; then
  AC_DEFINE(ENDIANESS_IN_SYS_PARAM_H, 1, [define if sys/param.h defines the endiness])dnl
fi
])
