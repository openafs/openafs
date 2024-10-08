AC_DEFUN([OPENAFS_LINUX_KERNEL_HEADER_CHECKS],[
dnl Check for header files
AC_CHECK_LINUX_HEADER([cred.h])
AC_CHECK_LINUX_HEADER([exportfs.h])
AC_CHECK_LINUX_HEADER([freezer.h])
AC_CHECK_LINUX_HEADER([key-type.h])
AC_CHECK_LINUX_HEADER([semaphore.h])
AC_CHECK_LINUX_HEADER([seq_file.h])
AC_CHECK_LINUX_HEADER([sched/signal.h])
AC_CHECK_LINUX_HEADER([uaccess.h])
AC_CHECK_LINUX_HEADER([stdarg.h])
dnl Linux 6.3 relocated file locking related declarations into it's own header
AC_CHECK_LINUX_HEADER([filelock.h])
])
