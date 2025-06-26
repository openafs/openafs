AC_DEFUN([OPENAFS_LINUX_KERNEL_SIG_CHECKS],[
dnl Operation signature checks
AC_CHECK_LINUX_OPERATION([inode_operations], [follow_link], [no_nameidata],
                         [#include <linux/fs.h>],
                         [const char *],
                         [struct dentry *dentry, void **link_data])
AC_CHECK_LINUX_OPERATION([inode_operations], [put_link], [no_nameidata],
                         [#include <linux/fs.h>],
                         [void],
                         [struct inode *inode, void *link_data])
AC_CHECK_LINUX_OPERATION([inode_operations], [rename], [takes_flags],
                         [#include <linux/fs.h>],
                         [int],
                         [struct inode *oinode, struct dentry *odentry,
                         struct inode *ninode, struct dentry *ndentry,
                         unsigned int flags])
dnl Linux 5.12 added the user_namespace parameter to the several
dnl inode operations functions.
dnl Perform a generic test using the inode_op create to test for this change.
AC_CHECK_LINUX_OPERATION([inode_operations], [create], [user_namespace],
                         [#include <linux/fs.h>],
                         [int],
                         [struct user_namespace *mnt_userns,
                         struct inode *inode, struct dentry *dentry,
                         umode_t umode, bool flag])
dnl if HAVE_LINUX_INODE_OPERATIONS_CREATE_USER_NAMESPACE, create a more generic
dnl define.
AS_IF([test AS_VAR_GET([ac_cv_linux_operation_inode_operations_create_user_namespace]) = yes],
      [AC_DEFINE([IOP_TAKES_USER_NAMESPACE], 1,
                 [define if inodeops require struct user_namespace])])
dnl Linux 6.3 replaced the user_namespace parameter with mnt_idmap for
dnl the inode operations functions.
AC_CHECK_LINUX_OPERATION([inode_operations], [create], [mnt_idmap],
                         [#include <linux/fs.h>],
                         [int],
                         [struct mnt_idmap *idmap,
                         struct inode *inode, struct dentry *dentry,
                         umode_t umode, bool flag])
dnl if HAVE_LINUX_INODE_OPERATIONS_CREATE_MNT_IDMAP, create a more generic
dnl define.
AS_IF([test AS_VAR_GET([ac_cv_linux_operation_inode_operations_create_mnt_idmap]) = yes],
      [AC_DEFINE([IOP_TAKES_MNT_IDMAP], 1,
                 [define if inodeops require struct mnt_idmap])])

dnl Linux 6.15 changed the return value for inode_operations.mkdir to a struct dentry *
AC_CHECK_LINUX_OPERATION([inode_operations], [mkdir], [return_dentry],
                         [#include <linux/fs.h>],
                         [struct dentry *],
                         [struct mnt_idmap *idmap,
                         struct inode *inode, struct dentry *dentry,
                         umode_t umode])

])