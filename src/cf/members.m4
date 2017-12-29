AC_DEFUN([OPENAFS_MEMBER_CHECKS],[
dnl see what struct stat has for timestamps
AC_CHECK_MEMBERS([struct stat.st_ctimespec, struct stat.st_ctimensec])
])
