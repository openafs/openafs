AC_DEFUN([LINUX_FS_STRUCT_ADDRESS_SPACE_HAS_GFP_MASK], [
AC_MSG_CHECKING(for gfp_mask in struct address_space)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_address_space_has_gfp_mask, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct address_space _a;
printf("%d\n", _a.gfp_mask);], 
ac_cv_linux_fs_struct_address_space_has_gfp_mask=yes,
ac_cv_linux_fs_struct_address_space_has_gfp_mask=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_address_space_has_gfp_mask)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_BYTES], [
AC_MSG_CHECKING(for i_bytes in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
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

AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_ALLOC_SEM], [
AC_MSG_CHECKING(for i_alloc_sem in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_alloc_sem,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _i;
printf("%x\n", _i.i_alloc_sem);], 
ac_cv_linux_fs_struct_inode_has_i_alloc_sem=yes,
ac_cv_linux_fs_struct_inode_has_i_alloc_sem=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_alloc_sem)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_FS_STRUCT_INODE_HAS_I_TRUNCATE_SEM], [
AC_MSG_CHECKING(for i_truncate_sem in struct inode)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_inode_has_i_truncate_sem,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _i;
printf("%x\n", _i.i_truncate_sem);], 
ac_cv_linux_fs_struct_inode_has_i_truncate_sem=yes,
ac_cv_linux_fs_struct_inode_has_i_truncate_sem=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_inode_has_i_truncate_sem)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_FS_STRUCT_ADDRESS_SPACE_HAS_PAGE_LOCK], [
AC_MSG_CHECKING(for page_lock in struct address_space)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_fs_struct_address_space_has_page_lock, 
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct address_space _a_s;
printf("%x\n", _a_s.page_lock);], 
ac_cv_linux_fs_struct_address_space_has_page_lock=yes,
ac_cv_linux_fs_struct_address_space_has_page_lock=no)])
AC_MSG_RESULT($ac_cv_linux_fs_struct_address_space_has_page_lock)
CPPFLAGS="$save_CPPFLAGS"])


dnl LINUX_BUILD_VNODE_FROM_INODE (configdir, outputdir, tmpldir)
dnl		defaults: (src/config, src/afs/LINUX, src/afs/linux)

AC_DEFUN([LINUX_BUILD_VNODE_FROM_INODE], [
AC_MSG_CHECKING(whether to build osi_vfs.h)
configdir=ifelse([$1], ,[src/config],$1)
outputdir=ifelse([$2], ,[src/afs/LINUX],$2)
tmpldir=ifelse([$3], ,[src/afs/LINUX],$3)
mkdir -p $outputdir
cp  $tmpldir/osi_vfs.hin $outputdir/osi_vfs.h
# chmod +x $configdir/make_vnode.pl
# $configdir/make_vnode.pl -i $LINUX_KERNEL_PATH -t ${tmpldir} -o $outputdir
])
