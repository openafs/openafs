AC_DEFUN([LINUX_INODE_SETATTR_RETURN_TYPE],[
AC_MSG_CHECKING(for inode_setattr return type)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_func_inode_setattr_returns_int,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode; 
struct iattr _iattr;
int i; 
i = inode_setattr(&_inode, &_iattr);], 
ac_cv_linux_func_inode_setattr_returns_int=yes,
ac_cv_linux_func_inode_setattr_returns_int=no)])
AC_MSG_RESULT($ac_cv_linux_func_inode_setattr_returns_int)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_WRITE_INODE_RETURN_TYPE],[
AC_MSG_CHECKING(for write_inode return type)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_func_write_inode_returns_int,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>],
[struct inode _inode; 
struct super_operations _sops;
int i; 
i = _sops.write_inode(&_inode, 0);], 
ac_cv_linux_func_write_inode_returns_int=yes,
ac_cv_linux_func_write_inode_returns_int=no)])
AC_MSG_RESULT($ac_cv_linux_func_write_inode_returns_int)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_IOP_NAMEIDATA],[
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_MSG_CHECKING(whether inode_operations.create takes a nameidata)
AC_CACHE_VAL(ac_cv_linux_func_i_create_takes_nameidata,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>
#include <linux/namei.h>],
[struct inode _inode; 
struct dentry _dentry;
struct nameidata _nameidata;
(void)_inode.i_op->create(&_inode, &_dentry, 0, &_nameidata);],
ac_cv_linux_func_i_create_takes_nameidata=yes,
ac_cv_linux_func_i_create_takes_nameidata=no)])
AC_MSG_RESULT($ac_cv_linux_func_i_create_takes_nameidata)
if test "x$ac_cv_linux_func_i_create_takes_nameidata" = "xyes" ; then
AC_DEFINE(IOP_CREATE_TAKES_NAMEIDATA, 1, [define if your iops.create takes a nameidata argument])
fi
AC_MSG_CHECKING(whether inode_operations.lookup takes a nameidata)
AC_CACHE_VAL(ac_cv_linux_func_i_lookup_takes_nameidata,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>
#include <linux/namei.h>],
[struct inode _inode; 
struct dentry _dentry;
struct nameidata _nameidata;
(void)_inode.i_op->lookup(&_inode, &_dentry, &_nameidata);],
ac_cv_linux_func_i_lookup_takes_nameidata=yes,
ac_cv_linux_func_i_lookup_takes_nameidata=no)])
AC_MSG_RESULT($ac_cv_linux_func_i_lookup_takes_nameidata)
if test "x$ac_cv_linux_func_i_lookup_takes_nameidata" = "xyes" ; then
AC_DEFINE(IOP_LOOKUP_TAKES_NAMEIDATA, 1, [define if your iops.lookup takes a nameidata argument])
fi
AC_MSG_CHECKING(whether inode_operations.permission takes a nameidata)
AC_CACHE_VAL(ac_cv_linux_func_i_permission_takes_nameidata,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>
#include <linux/namei.h>],
[struct inode _inode; 
struct nameidata _nameidata;
(void)_inode.i_op->permission(&_inode, 0, &_nameidata);],
ac_cv_linux_func_i_permission_takes_nameidata=yes,
ac_cv_linux_func_i_permission_takes_nameidata=no)])
AC_MSG_RESULT($ac_cv_linux_func_i_permission_takes_nameidata)
if test "x$ac_cv_linux_func_i_permission_takes_nameidata" = "xyes" ; then
AC_DEFINE(IOP_PERMISSION_TAKES_NAMEIDATA, 1, [define if your iops.permission takes a nameidata argument])
fi
AC_MSG_CHECKING(whether dentry_operations.d_revalidate takes a nameidata)
CPPFLAGS="$CPPFLAGS -Werror"
AC_CACHE_VAL(ac_cv_linux_func_d_revalidate_takes_nameidata,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>
#include <linux/namei.h>],
[struct dentry _dentry; 
struct nameidata _nameidata;
(void)_dentry.d_op->d_revalidate(&_dentry, &_nameidata);],
ac_cv_linux_func_d_revalidate_takes_nameidata=yes,
ac_cv_linux_func_d_revalidate_takes_nameidata=no)])
AC_MSG_RESULT($ac_cv_linux_func_d_revalidate_takes_nameidata)
if test "x$ac_cv_linux_func_d_revalidate_takes_nameidata" = "xyes" ; then
  AC_DEFINE(DOP_REVALIDATE_TAKES_NAMEIDATA, 1, [define if your dops.d_revalidate takes a nameidata argument])
fi
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_AOP_WRITEBACK_CONTROL],[
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_MSG_CHECKING(whether address_space_operations.writepage takes a writeback_control)
AC_CACHE_VAL(ac_cv_linux_func_a_writepage_takes_writeback_control,
[
AC_TRY_COMPILE(
[#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/writeback.h>],
[struct address_space_operations _aops; 
struct page _page;
struct writeback_control _writeback_control;
(void)_aops.writepage(&_page, &_writeback_control);],
ac_cv_linux_func_a_writepage_takes_writeback_control=yes,
ac_cv_linux_func_a_writepage_takes_writeback_control=no)])
AC_MSG_RESULT($ac_cv_linux_func_a_writepage_takes_writeback_control)
if test "x$ac_cv_linux_func_a_writepage_takes_writeback_control" = "xyes" ; then
AC_DEFINE(AOP_WRITEPAGE_TAKES_WRITEBACK_CONTROL, 1, [define if your aops.writepage takes a struct writeback_control argument])
fi
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_REFRIGERATOR],[
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-${SUBARCH} -D__KERNEL__ $CPPFLAGS"
AC_MSG_CHECKING(whether refrigerator takes PF_FREEZE)
AC_CACHE_VAL(ac_cv_linux_func_refrigerator_takes_pf_freeze,
[
AC_TRY_COMPILE(
[#include <linux/sched.h>],
[
refrigerator(PF_FREEZE);
],
ac_cv_linux_func_refrigerator_takes_pf_freeze=yes,
ac_cv_linux_func_refrigerator_takes_pf_freeze=no)])
AC_MSG_RESULT($ac_cv_linux_func_refrigerator_takes_pf_freeze)
if test "x$ac_cv_linux_func_refrigerator_takes_pf_freeze" = "xyes" ; then
AC_DEFINE(LINUX_REFRIGERATOR_TAKES_PF_FREEZE, 1, [define if your refrigerator takes PF_FREEZE])
fi
CPPFLAGS="$save_CPPFLAGS"])
