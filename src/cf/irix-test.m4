AC_DEFUN([IRIX_SYS_SYSTM_H_HAS_MEM_FUNCS], [
  AC_CACHE_CHECK([for mem* in sys/systm.h],
                 [ac_cv_irix_sys_systm_h_has_mem_funcs],
 [
    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -D_KERNEL -D__STRING_H__"
    AC_TRY_COMPILE(
[#include <sys/types.h>
#include <sys/systm.h>],
[extern void     *memcpy(char *, const void *, size_t);
],
[ac_cv_irix_sys_systm_h_has_mem_funcs=no],
[ac_cv_irix_sys_systm_h_has_mem_funcs=yes])
    CPPFLAGS="$save_CPPFLAGS"
  ])
  AS_IF([test "$ac_cv_irix_sys_systm_h_has_mem_funcs" = "yes"],
        [AC_DEFINE(IRIX_HAS_MEM_FUNCS, 1,
		   [define if irix has memcpy and friends])])
])
