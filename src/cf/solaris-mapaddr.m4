dnl This checks if the map_addr() function lacks the 'vacalign' argument. It
dnl was removed some time around Solaris 11.4.
dnl
dnl Note that the map_addr() function has had arguments added in the past
dnl (before Solaris 10). This check then only makes sense for newer Solaris;
dnl don't rely on it for pre-10 Solaris releases.
dnl
AC_DEFUN([SOLARIS_MAPADDR_LACKS_VACALIGN],
 [AC_CACHE_CHECK([for a map_addr without vacalign],
   [ac_cv_solaris_mapaddr_lacks_vacalign],
   [AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
       [[#define _KERNEL
         #include <sys/vmsystm.h>]],
       [[caddr_t *addrp;
         size_t len;
         offset_t off;
         uint_t flags;
         map_addr(addrp, len, off, flags);]])],
     [ac_cv_solaris_mapaddr_lacks_vacalign=yes],
     [ac_cv_solaris_mapaddr_lacks_vacalign=no])])

  AS_IF([test "x$ac_cv_solaris_mapaddr_lacks_vacalign" = "xyes"],
        [AC_DEFINE([MAPADDR_LACKS_VACALIGN], [1],
                   [define if the function map_addr lacks the vacalign argument])])])
