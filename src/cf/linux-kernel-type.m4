AC_DEFUN([OPENAFS_LINUX_KERNEL_TYPE_CHECKS],[
dnl Type existence checks
AC_CHECK_LINUX_TYPE([struct vfs_path], [dcache.h])
AC_CHECK_LINUX_TYPE([kuid_t], [uidgid.h])
AC_CHECK_LINUX_TYPE([struct proc_ops], [proc_fs.h])
])
