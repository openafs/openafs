
AC_DEFUN([OPENAFS_GCC_SUPPORTS_MARCH], [
AC_MSG_CHECKING(if $CC accepts -march=pentium)
save_CFLAGS="$CFLAGS"
CFLAGS="-MARCH=pentium"
AC_CACHE_VAL(openafs_cv_gcc_supports_march,[
AC_TRY_COMPILE(
[],
[int x;],
openafs_cv_gcc_supports_march=yes,
openafs_cv_gcc_supports_march=no)])
AC_MSG_RESULT($openafs_cv_gcc_supports_march)
if test x$openafs_cv_gcc_supports_march = xyes; then
  P5PLUS_KOPTS="-march=pentium"
else
  P5PLUS_KOPTS="-m486 -malign-loops=2 -malign-jumps=2 -malign-functions=2"
fi
CFLAGS="$save_CFLAGS"
])

AC_DEFUN([OPENAFS_GCC_NEEDS_NO_STRICT_ALIASING], [
AC_MSG_CHECKING(if $CC needs -fno-strict-aliasing)
save_CFLAGS="$CFLAGS"
CFLAGS="-fno-strict-aliasing"
AC_CACHE_VAL(openafs_cv_gcc_needs_no_strict_aliasing,[
AC_TRY_COMPILE(
[],
[int x;],
openafs_cv_gcc_needs_no_strict_aliasing=yes,
openafs_cv_gcc_needs_no_strict_aliasing=no)])
AC_MSG_RESULT($openafs_cv_gcc_needs_no_strict_aliasing)
if test x$openafs_cv_gcc_needs_no_strict_aliasing = xyes; then
  LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -fno-strict-aliasing"
fi
CFLAGS="$save_CFLAGS"
])

AC_DEFUN([OPENAFS_GCC_NEEDS_NO_STRENGTH_REDUCE], [
AC_MSG_CHECKING(if $CC needs -fno-strength-reduce)
save_CFLAGS="$CFLAGS"
CFLAGS="-fno-strength-reduce"
AC_CACHE_VAL(openafs_cv_gcc_needs_no_strength_reduce,[
AC_TRY_COMPILE(
[],
[int x;],
openafs_cv_gcc_needs_no_strength_reduce=yes,
openafs_cv_gcc_needs_no_strength_reduce=no)])
AC_MSG_RESULT($openafs_cv_gcc_needs_no_strength_reduce)
if test x$openafs_cv_gcc_needs_no_strength_reduce = xyes; then
  LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -fno-strength-reduce"
fi
CFLAGS="$save_CFLAGS"
])

AC_DEFUN([OPENAFS_GCC_SUPPORTS_NO_COMMON], [
AC_MSG_CHECKING(if $CC supports -fno-common)
save_CFLAGS="$CFLAGS"
CFLAGS="-fno-common"
AC_CACHE_VAL(openafs_cv_gcc_supports_no_common,[
AC_TRY_COMPILE(
[],
[int x;],
openafs_cv_gcc_supports_no_common=yes,
openafs_cv_gcc_supports_no_common=no)])
AC_MSG_RESULT($openafs_cv_gcc_supports_no_common)
if test x$openafs_cv_gcc_supports_no_common = xyes; then
  LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -fno-common"
fi
CFLAGS="$save_CFLAGS"
])

AC_DEFUN([OPENAFS_GCC_SUPPORTS_PIPE], [
AC_MSG_CHECKING(if $CC supports -pipe)
save_CFLAGS="$CFLAGS"
CFLAGS="-pipe"
AC_CACHE_VAL(openafs_cv_gcc_supports_pipe,[
AC_TRY_COMPILE(
[],
[int x;],
openafs_cv_gcc_supports_pipe=yes,
openafs_cv_gcc_supports_pipe=no)])
AC_MSG_RESULT($openafs_cv_gcc_supports_pipe)
if test x$openafs_cv_gcc_supports_pipe = xyes; then
  LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -pipe"
fi
CFLAGS="$save_CFLAGS"
])
