AC_DEFUN(LINUX_FS_STRUCT_INODE_HAS_I_CDEV, [
AC_MSG_CHECKING(for i_cdev in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_cdev, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_cdev);], 
ac_cv_linux_fs_struct_inode_has_i_cdev=yes,
ac_cv_linux_fs_struct_inode_has_i_cdev=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_cdev)
CPPFLAGS="$save_CPPFLAGS"])
