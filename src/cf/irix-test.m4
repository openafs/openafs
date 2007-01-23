AC_DEFUN([IRIX_SYS_SYSTM_H_HAS_MEM_FUNCS], [
AC_MSG_CHECKING(for mem* in sys/systm.h)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -D_KERNEL -D__STRING_H__"
AC_CACHE_VAL(ac_cv_irix_sys_systm_h_has_mem_funcs,
[
AC_TRY_COMPILE(
[#include <sys/types.h>
#include <sys/systm.h>],
[
extern void     *memcpy(char *, const void *, size_t);
],
ac_cv_irix_sys_systm_h_has_mem_funcs=no,
ac_cv_irix_sys_systm_h_has_mem_funcs=yes)])
CPPFLAGS="$save_CPPFLAGS"
if test "$ac_cv_irix_sys_systm_h_has_mem_funcs" = "yes"; then
  AC_DEFINE([IRIX_HAS_MEM_FUNCS], 1, [define if irix has memcpy and friends])
fi
AC_MSG_RESULT($ac_cv_irix_sys_systm_h_has_mem_funcs)
])
