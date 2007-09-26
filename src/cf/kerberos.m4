dnl
dnl $Id$
dnl
dnl Kerberos autoconf glue
dnl

AC_DEFUN([OPENAFS_KRB5CONF],[

dnl AC_ARG_VAR(KRB5CFLAGS, [C flags to compile Kerberos 5 programs])
dnl AC_ARG_VAR(KRB5LIBS, [Libraries and flags to compile Kerberos 5 programs])
dnl AC_ARG_VAR(KRB5CONFIG_SCRIPT, [Location of krb5-config script])
dnl AC_ARG_VAR(KRB5VENDOR, [Kerberos flavor--HEIMDAL or MIT])

AC_ARG_WITH([krb5-conf],[--with-krb5-conf[=krb5-config-location]    Use a krb5-config script to configure Kerberos])
AC_ARG_WITH([krb5], [--with-krb5 Support for Kerberos 5 (manual configuration)])

NEED_NFOLD='#'
NEED_DANISH='#'
NEED_RXK5_FIXUPS='#'
BUILD_KRB5=no
if test "X$with_krb5_conf" != X && test "X$with_krb5_conf" != Xno; then
	KRB5CONFIG_SCRIPT=$with_krb5_conf
	BUILD_KRB5=yes
dnl AC_MSG_RESULT([case 1 $KRB5CONFIG_SCRIPT for krb5_config XXX])
else
	if test -x "$with_krb5/bin/krb5-config";  then
		KRB5CONFIG_SCRIPT="$with_krb5/bin/krb5-config"
		BUILD_KRB5=yes
dnl AC_MSG_RESULT([case 2 $KRB5CONFIG_SCRIPT for krb5_config XXX])
	else
		if test "X$with_krb5" != X && test "X$with_krb5" != Xno; then
			BUILD_KRB5=yes
dnl AC_MSG_RESULT([case 3 do k5, manual config, or ssl XXX])
		else
		if test "X$conf_ssl" = XYES; then
			BUILD_KRB5=yes
dnl AC_MSG_RESULT([case 4 k4, ssl XXX])
else AC_MSG_RESULT([no case, without krb5 and not k5ssl XXX])
		fi
		fi
	fi
fi
if test "X$with_krb5_conf" = Xyes; then
	AC_PATH_PROG(KRB5CONFIG_SCRIPT, krb5-config, not_found)
	if test "X$KRB5CONFIG_SCRIPT" = Xnot_found &&
			test "X$conf_ssl" != XYES &&
			test "X$KRB5CFLAGS" = X &&
			test "X$KRB5LIBS" = X &&
			test "X$KRB5CONFIG_SCRIPT" = X; then
		AC_MSG_ERROR([cannot find krb5-config script, you must configure Kerberos manually])
	fi
	BUILD_KRB5=yes
fi
if test "X$KRB5CONFIG_SCRIPT" != X; then
	if test "X$conf_ssl" = XYES; then
		AC_MSG_ERROR([--with-ssl and $KRB5CONFIG_SCRIPT, choose only one])
	fi
	KRB5CFLAGS="`$KRB5CONFIG_SCRIPT --cflags krb5`"
	retval=$?
	if test $retval -ne 0; then
		AC_MSG_ERROR([$KRB5CONFIG_SCRIPT --cflags krb5: failed with an error code of $retval])
	fi
	KRB5LIBS_RAW="`$KRB5CONFIG_SCRIPT --libs krb5`"
	retval=$?
	if test $retval -ne 0; then
		AC_MSG_ERROR([$KRB5CONFIG_SCRIPT --libs krb5: failed with an error code of $retval])
	fi
	KRB5LIBS="`echo $KRB5LIBS_RAW | sed 's; [[^ ]]*com_err[[^ ]]*;;'`"
	KRB5PREFIX="`$KRB5CONFIG_SCRIPT --prefix`"
	retval=$?
	if test $retval -ne 0; then
		AC_MSG_ERROR([$KRB5CONFIG_SCRIPT --prefix: failed with an error code of $retval])
	fi
	AC_MSG_RESULT([Adding $KRB5CFLAGS to KRB5CFLAGS])
	AC_MSG_RESULT([k5libs $KRB5LIBS_RAW before removing -lcom_err])
	AC_MSG_RESULT([Adding $KRB5LIBS to KRB5LIBS])
	AC_MSG_RESULT([Setting $KRB5PREFIX to KRB5PREFIX])
fi
if test "X$BUILD_KRB5" = Xyes; then
if test "X$conf_ssl" = XYES; then
dnl AC_MSG_RESULT([set vendor K5SSL XXX])
	KRB5VENDOR="K5SSL";
else
dnl if krb5-config is missing, this is worth a try.
if test "X$KRB5CONFIG_SCRIPT" = X &&
		test "X$with_krb5" != Xyes; then
	if test "X$KRB5CFLAGS" = X; then
		KRB5CFLAGS="-I$with_krb5/include"
		AC_MSG_RESULT([Adding $KRB5CFLAGS to KRB5CFLAGS (heuristic)])
	fi
	if test "X$KRB5PREFIX" = X; then
		KRB5PREFIX="-I$with_krb5"
		AC_MSG_RESULT([Adding $KRB5PREFIX to KRB5PREFIX (heuristic)])
	fi
fi
dnl AC_MSG_RESULT([not ssl, find out who is vendor XXX])
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $KRB5CFLAGS"
if test "X$BUILD_KRB5" = Xyes; then
AC_MSG_CHECKING(for heimdal style krb5_keyblock)
AC_CACHE_VAL(ac_cv_heimdal_style_krb5_keyblock,
[
AC_TRY_COMPILE([#include <krb5.h>], [krb5_keyblock _k;
printf("%d %d %d\n", (int)_k.keytype, (int)_k.keyvalue.length,
(int)_k.keyvalue.data);],
kludge_need_parse_units_h=no
ac_cv_heimdal_style_krb5_keyblock=yes,
AC_TRY_COMPILE(
[#include <parse_units.h>
#include <krb5.h>], [krb5_keyblock _k;
printf("%d %d %d\n", (int)_k.keytype, (int)_k.keyvalue.length,
(int)_k.keyvalue.data);],
kludge_need_parse_units_h=yes
ac_cv_heimdal_style_krb5_keyblock=yes,
ac_cv_heimdal_style_krb5_keyblock=no)
)])
AC_MSG_RESULT($ac_cv_heimdal_style_krb5_keyblock)
if test "x$ac_cv_heimdal_style_krb5_keyblock" = xyes; then
if test "x$kludge_need_parse_units_h" = xyes; then
AC_DEFINE([HAVE_PARSE_UNITS_H], 1, [define if heimdal krb5.h needs parse_units.h])
fi
KRB5VENDOR="HEIMDAL";
else
AC_MSG_CHECKING(for mit style krb5_keyblock)
AC_CACHE_VAL(ac_cv_mit_style_krb5_keyblock,
[
AC_TRY_COMPILE(
[#include <krb5.h>],
[krb5_keyblock _k;
printf("%d %d %d\n", (int)_k.enctype, (int)_k.length, (int)_k.contents);],
ac_cv_mit_style_krb5_keyblock=yes,
ac_cv_mit_style_krb5_keyblock=no)])
AC_MSG_RESULT($ac_cv_mit_style_krb5_keyblock)
if test "x$ac_cv_mit_style_krb5_keyblock" = xyes; then
	KRB5VENDOR="MIT";
fi
fi
fi
dnl if krb5-config is missing, this is probably wrong, but worth a start.
if test "X$KRB5CONFIG_SCRIPT" = X &&
		test "X$with_krb5" != Xyes &&
		test "X$KRB5LIBS" = X; then
	if test "X$KRB5VENDOR" = XHEIMDAL; then
		KRB5LIBS="-L$with_krb5/lib -lkrb5 -lasn1 -lroken -lcrypto"
	else
	if test "X$KRB5VENDOR" = XMIT; then
		KRB5LIBS="-L$with_krb5/lib -lkrb5 -lk5crypto"
	else
		AC_MSG_WARN([-with-krb5, and unable to guess at KRB5LIBS])
	fi
	fi
	AC_MSG_RESULT([Adding $KRB5LIBS to KRB5LIBS (heuristic)])
fi
CPPFLAGS="$save_CPPFLAGS"
fi

if test "X${KRB5VENDOR}" != X; then
	if test "X${KRB5VENDOR}" != XK5SSL; then
		AC_MSG_RESULT([Detected $KRB5VENDOR Kerberos V implementation])
	fi
else
	AC_MSG_RESULT([Can't determine Kerberos V implementation])
fi
if test "X$KRB5VENDOR" = XHEIMDAL; then
	AC_DEFINE([COMPILED_WITH_HEIMDAL], 1, [define if linking against kth heimdal (please do not use this symbol for conditional compilation)])
	# TEMPORARY workaround to incompatibility of
	# AFS and Heimdal errortables
	if test "X$KRB5PREFIX" = X; then
		FIXUP_K5LIBDIR="/usr/lib"
	else
		FIXUP_K5LIBDIR="$KRB5PREFIX/lib"
	fi
	LIBFIXUPKRB5=libfixupkrb5.a
	NEED_RXK5_FIXUPS=''
	NEED_NFOLD=''
	K5SUPPORT=' nfold.o'
fi
if test "X$KRB5VENDOR" = XSHISHI; then
dnl *** Unsupported; only rxk5 has the necessary logic.
dnl *** Beware shishi licensing.
	AC_DEFINE([COMPILED_WITH_SHISHI], 1, [define if linking against shishi kerberos 5 (please do not use this symbol for conditional compilation)])
	NEED_NFOLD=''
	K5SUPPORT=' nfold.o'
fi
if test "X$KRB5VENDOR" = XMIT; then
	AC_DEFINE([COMPILED_WITH_MIT], 1, [define if linking against MIT kerberos 5 (please do not use this symbol for conditional compilation)])
	K5SUPPORT=' danish.o nfold.o'
	NEED_DANISH=''
	NEED_NFOLD=''
fi
if test "X$KRB5VENDOR" = XK5SSL; then
	AC_DEFINE([COMPILED_WITH_SSL], 1, [define if using k5ssl + openssl (please do not use this symbol for conditional compilation)])
	K5SUPPORT=''
	if test "X$KRB5CFLAGS" != X; then
		AC_MSG_WARN([-with-ssl, but KRB5CFLAGS is set])
	fi
	if test "X$KRB5LIBS" != X; then
		AC_MSG_WARN([-with-ssl, but KRB5LIBS is set])
	fi
	KRB5LIBS='${TOP_LIBDIR}/libk5ssl.a '"$SSLLIBS"' ${TOP_LIBDIR}/libcom_err.a'
	AC_MSG_RESULT([Using internal K5SSL Kerberos V implementation])
	AC_DEFINE([HAVE_KRB5_CREDS_KEYBLOCK], 1, [define if krb5_creds has keyblock])
	AC_DEFINE([HAVE_KRB5_PRINC_SIZE], 1, [define if krb5_princ_size exists])
fi


if test "X$KRB5VENDOR" != X && test "X$KRB5VENDOR" != XK5SSL; then
	AC_MSG_RESULT([Configuring support for Kerberos 5 utilities])
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
	
dnl	AC_CHECK_MEMBERS([krb5_creds.keyblock, krb5_creds.session],,, [#include <krb5.h>])
	CPPFLAGS="$save_CPPFLAGS"
	LIBS="$save_LIBS"
fi
if test "X$KRB5VENDOR" = XK5SSL; then
	AC_MSG_RESULT([Configuring built-in support for Kerberos 5])
	ac_cv_krb5_creds_keyblock_exists=yes;
	AC_DEFINE([HAVE_KRB5_PRINC_SIZE], 1, [define if krb5_princ_size exists])
fi

if test "x$ac_cv_krb5_creds_keyblock_exists" = xyes; then
	AC_DEFINE([HAVE_KRB5_CREDS_KEYBLOCK], 1, [define if krb5_creds has keyblock])
fi
if test "x$ac_cv_krb5_creds_session_exists" = xyes; then
	AC_DEFINE([HAVE_KRB5_CREDS_SESSION], 1, [define if krb5_creds has session])
fi
else AC_MSG_RESULT([Krb5 not configured. XXX])
fi

KAUTH_KLOG_SUFFIX=''
if test "$BUILD_KRB5" = "yes"; then
	KAUTH_KLOG_SUFFIX='.ka'
	DISABLE_KRB5='#'
else
	ENABLE_KRB5='#'
fi

AC_SUBST(DISABLE_KRB5)
AC_SUBST(ENABLE_KRB5)
AC_SUBST(KRB5CFLAGS)
AC_SUBST(KRB5LIBS)
dnl KRB5LIBS_RAW is not used; only output in case -lcom_err is missed.
AC_SUBST(KRB5LIBS_RAW)
AC_SUBST(FIXUP_K5LIBDIR)
AC_SUBST(LIBFIXUPKRB5)
AC_SUBST(K5SUPPORT)
AC_SUBST(NEED_RXK5_FIXUPS)
AC_SUBST(NEED_NFOLD)
AC_SUBST(NEED_DANISH)
AC_SUBST(KAUTH_KLOG_SUFFIX)

])dnl
