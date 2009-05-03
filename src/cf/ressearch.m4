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
  AC_TRY_LINK([
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#include <resolv.h>],
  [
struct __res_state nstate[1];
unsigned char reply[1024];
int r;
memset(nstate, 0, sizeof *nstate);
r = res_ninit(nstate);
r = res_nsearch(nstate, "openafs.org", C_IN, T_SRV, reply, sizeof reply);
res_nclose(nstate);
return 0;
  ],
  ac_cv_func_res_nclose=yes)
])
