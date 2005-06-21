dnl
dnl $Id$
dnl
dnl Kerberos autoconf glue
dnl

AC_DEFUN([OPENAFS_KRB5CONF],[

AC_ARG_VAR(KRB5CFLAGS, [C flags to compile Kerberos 5 programs])
AC_ARG_VAR(KRB5LIBS, [Libraries and flags to compile Kerberos 5 programs])
AC_ARG_VAR(KRB5_CONFIG, [Location of krb5-config script])

AC_ARG_WITH([krb5-conf],
	AC_HELP_STRING([--with-krb5-config[=krb5-config-location]],
		       [Use a krb5-config script to configure Kerberos]),
	[if test X$withval != Xno; then
		conf_krb5=YES
		if test X$withval = Xyes; then
			AC_PATH_PROG(KRB5_CONFIG, krb5-config, not_found)
			if test X$KRB5_CONFIG = Xnot_found; then
				AC_MSG_ERROR([cannot find krb5-config script, you must configure Kerberos manually])
			fi
		else
			KRB5_CONFIG=$withval
		fi
		KRB5CFLAGS=`$KRB5_CONFIG --cflags krb5`
		retval=$?
		if test $retval -ne 0; then
			AC_MSG_ERROR([$KRB5_CONFIG failed with an error code of $retval])
		fi
		KRB5LIBS=`$KRB5_CONFIG --libs krb5`
		retval=$?
		if test $retval -ne 0; then
			AC_MSG_ERROR([$KRB5_CONFIG failed with an error code of $retval])
		fi
		AC_MSG_RESULT([Adding $KRB5CFLAGS to KRB5CFLAGS])
		AC_MSG_RESULT([Adding $KRB5LIBS to KRB5LIBS])
	fi])

AC_ARG_WITH([krb5],
	AC_HELP_STRING([--with-krb5],
		       [Support for Kerberos 5 (manual configuration)]),
	[if test X$conf_krb5 = XYES; then
		AC_MSG_ERROR([--with-krb5-config and --with-krb5 are mutually exclusive, choose only one])
	fi
	if test "X$KRB5CFLAGS" = X; then
		AC_MSG_WARN([KRB5CFLAGS is not set])
	fi
	if test "X$KRB5LIBS" = X; then
		AC_MSG_WARN([KRB5LIBS is not set])
	fi
	conf_krb5=YES])

BUILD_KRB5=no
if test X$conf_krb5 = XYES; then
	AC_MSG_RESULT([Configuring support for Kerberos 5 utilities])
	BUILD_KRB5=yes
	save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS KRB5CFLAGS"
	save_LIBS="$LIBS"
	LIBS="$LIBS $KRB5LIBS"
	AC_CHECK_FUNCS([add_to_error_table])
	CFLAGS="$save_CFLAGS"
	LIBS="$save_LIBS"
fi
AC_SUBST(BUILD_KRB5)
])dnl
