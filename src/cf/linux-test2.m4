AC_DEFUN(LINUX_FS_STRUCT_INODE_HAS_I_BYTES, [
AC_MSG_CHECKING(for i_bytes in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_bytes, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_bytes);], 
ac_cv_linux_fs_struct_inode_has_i_bytes=yes,
ac_cv_linux_fs_struct_inode_has_i_bytes=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_bytes)
CPPFLAGS="$save_CPPFLAGS"])

