AC_DEFUN([SOLARIS_FS_HAS_FS_ROLLED], [
AC_MSG_CHECKING(for fs_rolled in struct proc)
AC_CACHE_VAL(ac_cv_solaris_fs_has_fs_rolled,
[
AC_TRY_COMPILE(
[#include <sys/fs/ufs_fs.h>],
[struct fs _fs;
(void) _fs.fs_rolled;], 
ac_cv_solaris_fs_has_fs_rolled=yes,
ac_cv_solaris_fs_has_fs_rolled=no)])
AC_MSG_RESULT($ac_cv_solaris_fs_has_fs_rolled)
if test "$ac_cv_solaris_fs_has_fs_rolled" = "yes"; then
  AC_DEFINE([STRUCT_FS_HAS_FS_ROLLED], 1, [define if struct fs has fs_rolled])
fi
])

