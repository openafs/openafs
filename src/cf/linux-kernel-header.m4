AC_DEFUN([OPENAFS_LINUX_KERNEL_HEADER_CHECKS],[
dnl Check for header files
AC_CHECK_LINUX_HEADER([cred.h])
AC_CHECK_LINUX_HEADER([config.h])
AC_CHECK_LINUX_HEADER([completion.h])
AC_CHECK_LINUX_HEADER([exportfs.h])
AC_CHECK_LINUX_HEADER([freezer.h])
AC_CHECK_LINUX_HEADER([key-type.h])
AC_CHECK_LINUX_HEADER([semaphore.h])
AC_CHECK_LINUX_HEADER([seq_file.h])
AC_CHECK_LINUX_HEADER([sched/signal.h])
AC_CHECK_LINUX_HEADER([uaccess.h])
])
