AC_DEFUN(LINUX_EXPORTS_TASKLIST_LOCK, [
AC_MSG_CHECKING(for exported tasklist_lock)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_tasklist_lock,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_tasklist_lock
#error tasklist_lock not exported
#endif],
ac_cv_linux_exports_tasklist_lock=yes,
ac_cv_linux_exports_tasklist_lock=no)])
AC_MSG_RESULT($ac_cv_linux_exports_tasklist_lock)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN(LINUX_EXPORTS_SYS_CALL_TABLE, [
AC_MSG_CHECKING(for exported sys_call_table)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_sys_call_table,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_sys_call_table
#error sys_call_table not exported
#endif],
ac_cv_linux_exports_sys_call_table=yes,
ac_cv_linux_exports_sys_call_table=no)])
AC_MSG_RESULT($ac_cv_linux_exports_sys_call_table)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN(LINUX_EXPORTS_INIT_MM, [
AC_MSG_CHECKING(for exported init_mm)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_init_mm,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_init_mm
#error init_mm not exported
#endif],
ac_cv_linux_exports_init_mm=yes,
ac_cv_linux_exports_init_mm=no)])
AC_MSG_RESULT($ac_cv_linux_exports_init_mm)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN(LINUX_EXPORTS_KALLSYMS_SYMBOL, [
AC_MSG_CHECKING(for exported kallsyms_symbol_to_address)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_kallsyms_symbol,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_kallsyms_symbol_to_address
#error kallsyms_symbol_to_address not exported
#endif],
ac_cv_linux_exports_kallsyms_symbol=yes,
ac_cv_linux_exports_kallsyms_symbol=no)])
AC_MSG_RESULT($ac_cv_linux_exports_kallsyms_symbol)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN(LINUX_EXPORTS_KALLSYMS_ADDRESS, [
AC_MSG_CHECKING(for exported kallsyms_address_to_symbol)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_exports_kallsyms_address,
[
AC_TRY_COMPILE(
[#include <linux/modversions.h>],
[#ifndef __ver_kallsyms_address_to_symbol
#error kallsyms_address_to_symbol not exported
#endif],
ac_cv_linux_exports_kallsyms_address=yes,
ac_cv_linux_exports_kallsyms_address=no)])
AC_MSG_RESULT($ac_cv_linux_exports_kallsyms_address)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN(LINUX_COMPLETION_H_EXISTS, [
AC_MSG_CHECKING(for linux/completion.h existance)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_completion_h_exists,
[
AC_TRY_COMPILE(
[#include <linux/completion.h>],
[struct completion _c;],
ac_cv_linux_completion_h_exists=yes,
ac_cv_linux_completion_h_exists=no)])
AC_MSG_RESULT($ac_cv_linux_completion_h_exists)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN(LINUX_FS_STRUCT_INODE_HAS_I_MMAP_SHARED, [
AC_MSG_CHECKING(for i_mmap_shared in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_mmap_shared,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_mmap_shared);],
ac_cv_linux_fs_struct_inode_has_i_mmap_shared=yes,
ac_cv_linux_fs_struct_inode_has_i_mmap_shared=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_mmap_shared)
CPPFLAGS="$save_CPPFLAGS"])

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

AC_DEFUN(LINUX_FS_STRUCT_INODE_HAS_I_DEVICES, [
AC_MSG_CHECKING(for i_devices in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_cdev, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode;
printf("%d\n", _inode.i_devices);], 
ac_cv_linux_fs_struct_inode_has_i_devices=yes,
ac_cv_linux_fs_struct_inode_has_i_devices=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_devices)
CPPFLAGS="$save_CPPFLAGS"])

