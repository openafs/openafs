dnl This checks for the existence of the vn_renamepath function, added in
dnl Solaris 11 and Solaris 10u8.
dnl
dnl Just trying to use the function is not sufficient for a configure test,
dnl since using an undeclared function is just a warning, and we want an error.
dnl Rather than try to rely on making warnings generate errors (which may
dnl change depending on what compiler version we're using, or in the future
dnl different compilers entirely), we detect the function by declaring an
dnl incompatible prototype. If that successfully compiles, vn_renamepath
dnl does not exist in the system headers, so we know it's not there. If it
dnl fails, we try to compile again without the incompatible prototype, to
dnl make sure we didn't fail for some other reason. If that succeeds, we know
dnl we have vn_renamepath available; if it fails, something else is wrong and
dnl we just try to proceed, assuming we don't have it.
dnl
AC_DEFUN([SOLARIS_HAVE_VN_RENAMEPATH],

 [AC_CACHE_CHECK([for vn_renamepath],
   [ac_cv_solaris_have_vn_renamepath],
   [AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
       [[#define _KERNEL
         #include <sys/vnode.h>]],
       [[void vn_renamepath(void);]])],
     [ac_cv_solaris_have_vn_renamepath=no],
     [AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM(
         [[#define _KERNEL
           #include <sys/vnode.h>]])],
       [ac_cv_solaris_have_vn_renamepath=yes],
       [ac_cv_solaris_have_vn_renamepath=no])])])

  AS_IF([test "x$ac_cv_solaris_have_vn_renamepath" = "xyes"],
        [AC_DEFINE([HAVE_VN_RENAMEPATH], [1],
                   [define if the function vn_renamepath exists])])])
