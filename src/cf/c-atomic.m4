AC_DEFUN([OPENAFS_C_ATOMIC_CHECKS], [
AC_CACHE_CHECK([if compiler has __sync_add_and_fetch],
    [ac_cv_sync_fetch_and_add],
    [AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[int var; return __sync_add_and_fetch(&var, 1);]])],[ac_cv_sync_fetch_and_add=yes],[ac_cv_sync_fetch_and_add=no])
])
AS_IF([test "$ac_cv_sync_fetch_and_add" = "yes"],
      [AC_DEFINE(HAVE_SYNC_FETCH_AND_ADD, 1,
          [define if your C compiler has __sync_add_and_fetch])])
])
