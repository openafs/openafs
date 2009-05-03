AC_DEFUN([SOLARIS_UFSVFS_HAS_DQRWLOCK], [
AC_MSG_CHECKING(for vfs_dqrwlock in struct ufsvfs)
AC_CACHE_VAL(ac_cv_solaris_ufsvfs_has_dqrwlock,
[
AC_TRY_COMPILE(
[#define _KERNEL
#include <sys/fs/ufs_inode.h>],
[struct ufsvfs _ufsvfs;
(void) _ufsvfs.vfs_dqrwlock;], 
ac_cv_solaris_ufsvfs_has_dqrwlock=yes,
ac_cv_solaris_ufsvfs_has_dqrwlock=no)])
AC_MSG_RESULT($ac_cv_solaris_ufsvfs_has_dqrwlock)
if test "$ac_cv_solaris_ufsvfs_has_dqrwlock" = "yes"; then
  AC_DEFINE([HAVE_VFS_DQRWLOCK], 1, [define if struct ufsvfs has vfs_dqrwlock])
fi
])

