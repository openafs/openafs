AC_DEFUN([AC_TYPE_SOCKLEN_T],
[
AC_CACHE_CHECK([for socklen_t], 
ac_cv_type_socklen_t, [
        AC_TRY_COMPILE([
                      #include <sys/types.h>
                      #include <sys/socket.h>
              ],
              [
                      socklen_t len = 42; return 0;
              ],
              ac_cv_type_socklen_t="yes", ac_cv_type_socklen_t="no")
        ])

        if test "x$ac_cv_type_socklen_t" = "xno"; then
              AC_DEFINE(socklen_t, int, [the type of the last argument to getsockopt etc])
        fi
])
