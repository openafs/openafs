AC_DEFUN([OPENAFS_PTHREAD_CHECKS],[
PTHREAD_LIBS=error
if test "x$MKAFS_OSTYPE" = OBSD; then
        PTHREAD_LIBS="-pthread"
fi
if test "x$MKAFS_OSTYPE" = xDFBSD; then
        PTHREAD_LIBS="-pthread"
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(pthread, pthread_attr_init,
                PTHREAD_LIBS="-lpthread")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(pthreads, pthread_attr_init,
                PTHREAD_LIBS="-lpthreads")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(c_r, pthread_attr_init,
                PTHREAD_LIBS="-lc_r")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_FUNC(pthread_attr_init, PTHREAD_LIBS="")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        # pthread_attr_init is a macro under HPUX 11.0 and 11.11
        AC_CHECK_LIB(pthread, pthread_attr_destroy,
                PTHREAD_LIBS="-lpthread")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_MSG_WARN(*** Unable to locate working posix thread library ***)
fi
AC_SUBST(PTHREAD_LIBS)
])
