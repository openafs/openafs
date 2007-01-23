dnl
dnl $Id$
dnl
dnl openssl autoconf glue
dnl

AC_DEFUN([OPENAFS_SSL],[

AC_ARG_WITH([ssl], [--with-ssl Support for SSL])

if test X$with_ssl != X; then
	conf_ssl=YES
	if test X$with_ssl != Xyes; then
		SSLINCL="-I$withval/include";
		SSLLIBS="-L$withval/lib -lcrypto";
	else
		SSLLIBS="-lcrypto";
	fi
	DISABLE_SSL='#'
else
	ENABLE_SSL='#'
fi

AC_SUBST(SSLINCL)
AC_SUBST(SSLLIBS)
AC_SUBST(ENABLE_SSL)
AC_SUBST(DISABLE_SSL)

])dnl
