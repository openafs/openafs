AC_DEFUN(LINUX_FS_STRUCT_INODE_HAS_I_DIRTY_DATA_BUFFERS, [
AC_MSG_CHECKING(for i_dirty_data_buffers in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_dirty_data_buffers);], 
ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers=yes,
ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN(LINUX_FS_STRUCT_INODE_HAS_I_MAPPING_OVERLOAD, [
AC_MSG_CHECKING(for i_mapping_overload in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_mapping_overload, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_mapping_overload);], 
ac_cv_linux_fs_struct_inode_has_i_mapping_overload=yes,
ac_cv_linux_fs_struct_inode_has_i_mapping_overload=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_mapping_overload)
CPPFLAGS="$save_CPPFLAGS"])

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

AC_DEFUN(LINUX_FS_STRUCT_INODE_HAS_I_TRUNCATE_SEM, [
AC_MSG_CHECKING(for i_truncate_sem in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_truncate_sem, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_truncate_sem);], 
ac_cv_linux_fs_struct_inode_has_i_truncate_sem=yes,
ac_cv_linux_fs_struct_inode_has_i_truncate_sem=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_truncate_sem)
CPPFLAGS="$save_CPPFLAGS"])
