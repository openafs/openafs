AC_DEFUN([OPENAFS_DIRENT_CHECKS],[
AC_MSG_CHECKING([checking for dirfd])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
]],
        [[DIR *d = 0; dirfd(d);]])],
        [ac_rk_have_dirfd=yes], [ac_rk_have_dirfd=no])
if test "$ac_rk_have_dirfd" = "yes" ; then
        AC_DEFINE_UNQUOTED(HAVE_DIRFD, 1, [have a dirfd function/macro])
fi
AC_MSG_RESULT($ac_rk_have_dirfd)

OPENAFS_HAVE_STRUCT_FIELD(DIR, dd_fd, [#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif])
])
