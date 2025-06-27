AC_DEFUN([OPENAFS_LINUX_KERNEL_SYMBOL_CHECKS],[
dnl Type existence checks

dnl Linux 2.6.38 introduced DCACHE_NEED_AUTOMOUNT, and Linux 6.15 converted it
dnl from a #define to an enum.
AC_CHECK_LINUX_SYMBOL([DCACHE_NEED_AUTOMOUNT], [dcache.h])
])
