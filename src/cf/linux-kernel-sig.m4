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
])
