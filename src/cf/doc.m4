AC_DEFUN([OPENAFS_DOC],[
dnl Check whether kindlegen exists.  If not, we'll suppress that part of the
dnl documentation build.
AC_CHECK_PROGS([KINDLEGEN], [kindlegen])
AC_CHECK_PROGS([DOXYGEN], [doxygen])
AC_CHECK_PROGS([PERL], [perl])

dnl Optionally generate graphs with doxygen.
case "$with_dot" in
maybe)
    AC_CHECK_PROGS([DOT], [dot])
    AS_IF([test "x$DOT" = "x"], [HAVE_DOT="no"], [HAVE_DOT="yes"])
    ;;
yes)
    HAVE_DOT="yes"
    ;;
no)
    HAVE_DOT="no"
    ;;
*)
    HAVE_DOT="yes"
    DOT_PATH=$with_dot
esac
AC_SUBST(HAVE_DOT)
AC_SUBST(DOT_PATH)
AC_SUBST(PERL)
])
