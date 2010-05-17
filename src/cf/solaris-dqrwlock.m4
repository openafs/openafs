AC_DEFUN([SOLARIS_UFSVFS_HAS_DQRWLOCK], [
  AC_CACHE_CHECK([for vfs_dqrwlock in struct ufsvfs],
    [ac_cv_solaris_ufsvfs_has_dqrwlock],
    [AC_TRY_COMPILE(
        [#define _KERNEL
#include <sys/fs/ufs_inode.h>],
	[struct ufsvfs _ufsvfs;
(void) _ufsvfs.vfs_dqrwlock;], 
	[ac_cv_solaris_ufsvfs_has_dqrwlock=yes],
	[ac_cv_solaris_ufsvfs_has_dqrwlock=no])
    ])
  AS_IF([test "$ac_cv_solaris_ufsvfs_has_dqrwlock" = "yes"],
	[AC_DEFINE(HAVE_VFS_DQRWLOCK, 1,
		   [define if struct ufsvfs has vfs_dqrwlock])
	])
])

