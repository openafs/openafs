AC_DEFUN([OPENAFS_PTHREAD_CHECKS],
[PTHREAD_LIBS=error
AS_IF([test "x$MKAFS_OSTYPE" = OBSD],
  [PTHREAD_LIBS="-pthread"])
AS_IF([test "x$MKAFS_OSTYPE" = xDFBSD],
  [PTHREAD_LIBS="-pthread"])
AS_IF([test "x$PTHREAD_LIBS" = xerror],
  [AC_CHECK_LIB([pthread], [pthread_attr_init],
                [PTHREAD_LIBS="-lpthread"])])
AS_IF(["x$PTHREAD_LIBS" = xerror],
  [AC_CHECK_LIB([pthreads], [pthread_attr_init],
                [PTHREAD_LIBS="-lpthreads"])])
AS_IF([test "x$PTHREAD_LIBS" = xerror],
  [AC_CHECK_LIB([c_r], [pthread_attr_init],
                [PTHREAD_LIBS="-lc_r"])])
AS_IF([test "x$PTHREAD_LIBS" = xerror],
  [AC_CHECK_FUNC([pthread_attr_init], [PTHREAD_LIBS=""])])
AS_IF([test "x$PTHREAD_LIBS" = xerror],
  [# pthread_attr_init is a macro under HPUX 11.0 and 11.11
   AC_CHECK_LIB([pthread], [pthread_attr_destroy],
                [PTHREAD_LIBS="-lpthread"])])
AS_IF([test "x$PTHREAD_LIBS" = xerror],
  [AC_MSG_WARN([*** Unable to locate working posix thread library ***])])
AC_SUBST([PTHREAD_LIBS])
]) # OPENAFS_PTHREADS_CHECKS

AC_DEFUN([OPENAFS_MORE_PTHREAD_CHECKS],
[dnl Look for "non-portable" pthreads functions.
save_LIBS="$LIBS"
LIBS="$LIBS $PTHREAD_LIBS"
AC_CHECK_FUNCS([ \
  pthread_set_name_np \
  pthread_setname_np \
])
dnl Sadly, there are three different versions of pthread_setname_np.
dnl Try to cater for all of them.
AS_IF([test "$ac_cv_func_pthread_setname_np" = "yes"],
  [AC_MSG_CHECKING([for signature of pthread_setname_np])
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
      [#include <pthread.h>
       #ifdef HAVE_PTHREAD_NP_H
       #include <pthread_np.h>
       #endif],
      [pthread_setname_np(pthread_self(), "test", (void *)0)])],
    [AC_MSG_RESULT([three arguments])
     pthread_setname_np_args=3],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
        [#include <pthread.h>
         #ifdef HAVE_PTHREAD_NP_H
         #include <pthread_np.h>
         #endif],
        [pthread_setname_np(pthread_self(), "test")])],
      [AC_MSG_RESULT([two arguments])
       pthread_setname_np_args=2],
      [AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
          [#include <pthread.h>
           #ifdef HAVE_PTHREAD_NP_H
           #include <pthread_np.h>
           #endif],
          [pthread_setname_np("test")])],
          [AC_MSG_RESULT([one argument])
           pthread_setname_np_args=1],
          [pthread_setname_np_args=0])])])
  AC_DEFINE_UNQUOTED(
    [PTHREAD_SETNAME_NP_ARGS],
    [$pthread_setname_np_args],
    [Number of arguments required by pthread_setname_np() function])])
]) # OPENAFS_MORE_PTHREAD_CHECKS
