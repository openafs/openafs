AC_DEFUN([OPENAFS_HPUX_CHECKS],[
case $AFS_SYSNAME in
*hp_ux* | *hpux*)
   AC_MSG_WARN([Some versions of HP-UX have a buggy positional I/O implementation. Forcing no positional I/O.])
   ;;
*)
   AC_MSG_CHECKING([for positional I/O])
   if test "$ac_cv_func_pread" = "yes" && \
           test "$ac_cv_func_pwrite" = "yes"; then
      AC_DEFINE(HAVE_PIO, 1, [define if you have pread() and pwrite()])
      AC_MSG_RESULT(yes)
   else
     AC_MSG_RESULT(no)
   fi
   AC_MSG_CHECKING([for vectored positional I/O])
   AS_IF([test "$ac_cv_func_preadv" = "yes" -a \
               "$ac_cv_func_pwritev" = "yes" -a \
	       "$ac_cv_func_preadv64" = "yes" -a \
	       "$ac_cv_func_pwritev64" = "yes"],
	 [AC_DEFINE(HAVE_PIOV, 1, [define if you have preadv() and pwritev()])
  	  AC_MSG_RESULT(yes)],
	 [AC_MSG_RESULT(no)])
   ;;
esac
])
