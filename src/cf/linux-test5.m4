dnl These options seem to only be used for the 2.4.x
dnl Linux kernel build
AC_DEFUN([OPENAFS_GCC_SUPPORTS_MARCH], [
  AC_CACHE_CHECK([if $CC accepts -march=pentium],
    [openafs_cv_gcc_supports_march],
    [save_CFLAGS="$CFLAGS"
     CFLAGS="-MARCH=pentium"
     AC_TRY_COMPILE([],
		    [int x;],
		    [openafs_cv_gcc_supports_march=yes],
		    [openafs_cv_gcc_supports_march=no])
     CFLAGS="$save_CFLAGS"
    ])
  AS_IF([test x$openafs_cv_gcc_supports_march = xyes],
        [P5PLUS_KOPTS="-march=pentium"],
        [P5PLUS_KOPTS="-m486 -malign-loops=2 -malign-jumps=2 -malign-functions=2"])
])

AC_DEFUN([OPENAFS_GCC_NEEDS_NO_STRICT_ALIASING], [
  AC_CACHE_CHECK([if $CC needs -fno-strict-aliasing],
    [openafs_cv_gcc_needs_no_strict_aliasing],
    [save_CFLAGS="$CFLAGS"
     CFLAGS="-fno-strict-aliasing"
     AC_TRY_COMPILE([],
		    [int x;],
		    [openafs_cv_gcc_needs_no_strict_aliasing=yes],
		    [openafs_cv_gcc_needs_no_strict_aliasing=no])
     CFLAGS="$save_CFLAGS"
  ])
  AS_IF([test x$openafs_cv_gcc_needs_no_strict_aliasing = xyes],
        [LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -fno-strict-aliasing"])
])

AC_DEFUN([OPENAFS_GCC_NEEDS_NO_STRENGTH_REDUCE], [
  AC_CACHE_CHECK([if $CC needs -fno-strength-reduce],
    [openafs_cv_gcc_needs_no_strength_reduce],
    [save_CFLAGS="$CFLAGS"
     CFLAGS="-fno-strength-reduce"
     AC_TRY_COMPILE([],
		    [int x;],
		    [openafs_cv_gcc_needs_no_strength_reduce=yes],
		    [openafs_cv_gcc_needs_no_strength_reduce=no])
     CFLAGS="$save_CFLAGS"
  ])
  AS_IF([test x$openafs_cv_gcc_needs_no_strength_reduce = xyes],
        [LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -fno-strength-reduce"])
])

AC_DEFUN([OPENAFS_GCC_SUPPORTS_NO_COMMON], [
  AC_CACHE_CHECK([if $CC supports -fno-common],
    [openafs_cv_gcc_supports_no_common],
    [save_CFLAGS="$CFLAGS"
     CFLAGS="-fno-common"
     AC_TRY_COMPILE([],
		    [int x;],
		    [openafs_cv_gcc_supports_no_common=yes],
		    [openafs_cv_gcc_supports_no_common=no])

     CFLAGS="$save_CFLAGS"
  ])
  AS_IF([test x$openafs_cv_gcc_supports_no_common = xyes],
	[LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -fno-common"])
])

AC_DEFUN([OPENAFS_GCC_SUPPORTS_PIPE], [
  AC_CACHE_CHECK([if $CC supports -pipe],
    [openafs_cv_gcc_supports_pipe],
    [save_CFLAGS="$CFLAGS"
     CFLAGS="-pipe"
     AC_TRY_COMPILE([],
		    [int x;],
		    [openafs_cv_gcc_supports_pipe=yes],
		    [openafs_cv_gcc_supports_pipe=no])
  CFLAGS="$save_CFLAGS"
  ])
  AS_IF([test x$openafs_cv_gcc_supports_pipe = xyes],
	  [LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -pipe"])
])
