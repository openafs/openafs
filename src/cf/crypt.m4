AC_DEFUN([OPENAFS_CRYPT_CHECKS],[
dnl Check to see if crypt lives in a different library
AC_CHECK_LIB(crypt, crypt, LIB_crypt="-lcrypt")
AC_SUBST(LIB_crypt)
])
