dnl
dnl $Id$
dnl
dnl Kerberos autoconf glue
dnl

AC_DEFUN([OPENAFS_KRB5CONF],[

dnl AC_ARG_VAR(KRB5CFLAGS, [C flags to compile Kerberos 5 programs])
dnl AC_ARG_VAR(KRB5LIBS, [Libraries and flags to compile Kerberos 5 programs])
dnl AC_ARG_VAR(KRB5_CONFIG, [Location of krb5-config script])

AC_ARG_WITH([krb5-conf],[--with-krb5-conf[=krb5-config-location]    Use a krb5-config script to configure Kerberos])
if test X$with_krb5_conf != X; then
		conf_krb5=YES
		if test X$with_krb5_conf = Xyes; then
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
fi

AC_ARG_WITH([krb5], [--with-krb5 Support for Kerberos 5 (manual configuration)])

if test X$with_krb5 = Xyes; then
        if test X$conf_krb5 = XYES; then
		AC_MSG_ERROR([--with-krb5-config and --with-krb5 are mutually exclusive, choose only one])
	fi
	if test "X$KRB5CFLAGS" = X; then
		AC_MSG_WARN([KRB5CFLAGS is not set])
	fi
	if test "X$KRB5LIBS" = X; then
		AC_MSG_WARN([KRB5LIBS is not set])
	fi
	conf_krb5=YES
fi

BUILD_KRB5=no
if test X$conf_krb5 = XYES; then
	AC_MSG_RESULT([Configuring support for Kerberos 5 utilities])
	BUILD_KRB5=yes
	save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $KRB5CFLAGS"
	save_LIBS="$LIBS"
	LIBS="$LIBS $KRB5LIBS"
	AC_CHECK_FUNCS([add_to_error_table add_error_table krb5_princ_size krb5_principal_get_comp_string encode_krb5_enc_tkt_part encode_krb5_ticket])
	AC_CHECK_FUNCS([krb5_524_convert_creds], ,
	    [AC_CHECK_FUNCS([krb524_convert_creds_kdc], ,
		[AC_CHECK_LIB([krb524], [krb524_convert_creds_kdc],
		    [LIBS="-lkrb524 $LIBS"
		     KRB5LIBS="-lkrb524 $LIBS"
		     AC_DEFINE([HAVE_KRB524_CONVERT_CREDS_KDC], 1,
			 [Define to 1 if you have the `krb524_convert_creds_kdc' function.])])])])
	AC_CHECK_HEADERS([kerberosIV/krb.h])
	AC_CHECK_HEADERS([kerberosV/heim_err.h])

AC_MSG_CHECKING(for krb5_creds.keyblock existence)
AC_CACHE_VAL(ac_cv_krb5_creds_keyblock_exists,
[
AC_TRY_COMPILE(
[#include <krb5.h>],
[krb5_creds _c;
printf("%x\n", _c.keyblock);], 
ac_cv_krb5_creds_keyblock_exists=yes,
ac_cv_krb5_creds_keyblock_exists=no)])
AC_MSG_RESULT($ac_cv_krb5_creds_keyblock_exists)
	
AC_MSG_CHECKING(for krb5_creds.session existence)
AC_CACHE_VAL(ac_cv_krb5_creds_session_exists,
[
AC_TRY_COMPILE(
[#include <krb5.h>],
[krb5_creds _c;
printf("%x\n", _c.session);], 
ac_cv_krb5_creds_session_exists=yes,
ac_cv_krb5_creds_session_exists=no)])
AC_MSG_RESULT($ac_cv_krb5_creds_session_exists)

if test "x$ac_cv_krb5_creds_keyblock_exists" = "xyes"; then
	AC_DEFINE(HAVE_KRB5_CREDS_KEYBLOCK, 1, [define if krb5_creds has keyblock])
fi
if test "x$ac_cv_krb5_creds_session_exists" = "xyes"; then
	AC_DEFINE(HAVE_KRB5_CREDS_SESSION, 1, [define if krb5_creds has session])
fi
	
dnl	AC_CHECK_MEMBERS([krb5_creds.keyblock, krb5_creds.session],,, [#include <krb5.h>])
	CPPFLAGS="$save_CPPFLAGS"
	LIBS="$save_LIBS"
fi

AC_SUBST(BUILD_KRB5)
AC_SUBST(KRB5CFLAGS)
AC_SUBST(KRB5LIBS)

])dnl
