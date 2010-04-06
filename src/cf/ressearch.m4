AC_DEFUN([AC_CHECK_RESOLV_RETRANS],[
  AC_CACHE_CHECK([for retransmit support in res_state], [ac_cv_res_retransretry],[
  AC_TRY_RUN( [
#include <sys/types.h>
#if defined(__sun__)
#include <inet/ip.h>
#endif
#include <resolv.h>
int main(void) {
    _res.retrans = 2;
    _res.retry = 1;
    exit(0);
}
],[
  ac_cv_res_retransretry=1
],[
  ac_cv_res_retransretry=
],[
  ac_cv_res_retransretry="no"
])])
AC_DEFINE_UNQUOTED([HAVE_RES_RETRANSRETRY], [$ac_cv_res_retransretry], [Define if resolv.h's res_state has the fields retrans/rety])
])

AC_DEFUN([AC_FUNC_RES_SEARCH], [
  ac_cv_func_res_search=no
  AC_TRY_LINK([
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#include <resolv.h>],
  [
const char host[11]="openafs.org";
u_char ans[1024];
int r;
res_init();
/* Capture result in r but return 0, since a working nameserver is
 * not a requirement for compilation.
 */
r =  res_search( host, C_IN, T_MX, (u_char *)&ans, sizeof(ans));
return 0;
  ],
  ac_cv_func_res_search=yes)
])
