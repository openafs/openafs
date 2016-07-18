AC_PREREQ([2.60])
AC_INIT([OpenAFS], m4_esyscmd([build-tools/git-version .]))
AC_CONFIG_AUX_DIR([build-tools])
AC_CONFIG_SRCDIR([src/config/stds.h])
AM_INIT_AUTOMAKE

AC_CONFIG_HEADER(src/config/afsconfig.h)
MACOS_VERSION=1.6.19d2
LINUX_PKGVER=1.6.18.2

dnl Debian wants the release candidate version in the main upstream version,
dnl and wants ~ before it.
DEB_PKGVER=m4_esyscmd([build-tools/git-version .])
if echo "$LINUX_PKGREL" | grep pre >/dev/null 2>/dev/null ; then
    DEB_PKGVER="$DEB_PKGVER"`echo "$LINUX_PKGREL" | sed 's/0.pre/~rc/'`
fi

AC_SUBST(MACOS_VERSION)
AC_SUBST(DEB_PKGVER)

dnl If the user hasn't specified CFLAGS don't let configure pick -g -O2
AS_IF([test -z "$CFLAGS"], [CFLAGS=" "], [])

AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CC

AC_PATH_PROGS([PATH_CPP], [cpp], [${CC-cc} -E], [$PATH:/lib:/usr/ccs/lib])
AC_SUBST([PATH_CPP])
OPENAFS_CONFIGURE_COMMON

dnl Probe for Kerberos.  We have a few platform-specific overrides due to
dnl weird Kerberos implementations and installation locations.
AS_CASE([$AFS_SYSNAME],
    [*_obsd*],
    [KRB5_CPPFLAGS="-I/usr/include/kerberosV"],

    [ppc_darwin_70],
    [KRB5_CPPFLAGS="-I/usr/include"
     KRB5_LDFLAGS="-L/usr/lib -Wl,-search_paths_first"])
RRA_LIB_KRB5_OPTIONAL
AS_CASE([$AFS_SYSNAME],
    [hp_ux*|*_hpux*],
    [KRB5_LIBS="-l:libkrb5.sl -l:libcom_err.sl"])

dnl Check for the characteristics of whatever Kerberos we found, if we found
dnl one.
BUILD_KRB5=no
MAKE_KRB5="#"
AS_IF([test x"$KRB5_LIBS" != x],
    [BUILD_KRB5=yes
     MAKE_KRB5=
     AC_DEFINE([USE_RXKAD_KEYTAB], 1,
	       [Define to 1 if krb5 libraries are available and rxkad can use keytabs])
     RRA_LIB_KRB5_SWITCH
     AC_CHECK_FUNCS([add_error_table \
        add_to_error_table \
        encode_krb5_enc_tkt_part \
        encode_krb5_ticket \
	krb5_524_conv_principal \
        krb5_allow_weak_crypto \
        krb5_c_decrypt \
        krb5_c_encrypt \
        krb5_crypto_init \
        krb5_decode_ticket \
        krb5_decrypt_tkt_part \
        krb5_encrypt_tkt_part \
        krb5_enctype_enable \
        krb5_get_init_creds_opt_alloc \
        krb5_get_prompt_types \
        krb5_princ_size \
        krb5_principal_get_comp_string])
     AC_CHECK_FUNCS([krb5_524_convert_creds], ,
         [AC_CHECK_FUNCS([krb524_convert_creds_kdc], ,
             [AC_CHECK_LIB([krb524], [krb524_convert_creds_kdc],
                 [LIBS="-lkrb524 $LIBS"
                  KRB5_LIBS="-lkrb524 $KRB5_LIBS"
		  AC_CHECK_LIB([krb524], [krb5_524_conv_principal],
		  [AC_DEFINE([HAVE_KRB5_524_CONV_PRINCIPAL], 1,
		     [Define to 1 if you have the `krb5_524_conv_principal' function.])])
                  AC_DEFINE([HAVE_KRB524_CONVERT_CREDS_KDC], 1,
                     [Define to 1 if you have the `krb524_convert_creds_kdc' function.])])])])
     AC_CHECK_HEADERS([kerberosIV/krb.h])
     AC_CHECK_HEADERS([kerberosV/heim_err.h])
     AC_CHECK_HEADERS([com_err.h et/com_err.h krb5/com_err.h])
     AS_IF([test x"$ac_cv_header_com_err_h" != xyes \
             && test x"$ac_cv_header_et_com_err_h" != xyes \
             && test x"$ac_cv_header_krb5_com_err_h" != xyes],
       [AC_MSG_ERROR([Cannot find a usable com_err.h])])
     AC_CHECK_MEMBERS([krb5_creds.keyblock, krb5_creds.keyblock.enctype,
		       krb5_creds.session, krb5_keytab_entry.key,
		       krb5_keytab_entry.keyblock, krb5_keyblock.enctype,
		       krb5_keyblock.keytype, krb5_prompt.type], , ,
		       [#include <krb5.h>])
dnl If we have krb5_creds.session, we are using heimdal
dnl If we're using heimdal, aklog needs libasn1 for encode_EncTicketPart and a
dnl few other functions. But just aklog; not any of the other stuff that uses
dnl krb5.
     AS_IF([test x"$ac_cv_member_krb5_creds_session" = xyes],
           [AC_CHECK_LIB([asn1], [encode_EncTicketPart],
	                [AKLOG_KRB5_LIBS="-lasn1"])])
     AC_CHECK_DECLS([krb5_free_keytab_entry_contents, krb5_kt_free_entry,
		    KRB5_KU_TICKET], [], [], [#include <krb5.h>])
     RRA_LIB_KRB5_RESTORE])
AC_SUBST([BUILD_KRB5])
AC_SUBST([MAKE_KRB5])
AC_SUBST([AKLOG_KRB5_LIBS])

if test -d 'doc/man-pages' ; then
    MAN_MAKEFILE="doc/man-pages/Makefile doc/man-pages/install-man"
else
    MAN_MAKEFILE=
fi
if test -f 'doc/xml/AdminGuide/Makefile.in' ; then
   ADMINGUIDE_MAKEFILE="doc/xml/AdminGuide/Makefile"
else
   ADMINGUIDE_MAKEFILE=
fi
if test -f 'doc/xml/QuickStartUnix/Makefile.in' ; then
   QSUNIX_MAKEFILE="doc/xml/QuickStartUnix/Makefile"
else
   QSUNIX_MAKEFILE=
fi
if test -f 'doc/xml/UserGuide/Makefile.in' ; then
   USERGUIDE_MAKEFILE="doc/xml/UserGuide/Makefile"
else
   USERGUIDE_MAKEFILE=
fi

AC_OUTPUT(             \
Makefile               \
${MAN_MAKEFILE} \
${ADMINGUIDE_MAKEFILE} \
${QSUNIX_MAKEFILE} \
${USERGUIDE_MAKEFILE} \
src/afs/Makefile \
src/afsd/Makefile \
src/afsmonitor/Makefile \
src/afsweb/Makefile \
src/aklog/Makefile \
src/audit/Makefile \
src/auth/Makefile \
src/auth/test/Makefile \
src/bozo/Makefile \
src/bozo/test/Makefile \
src/bu_utils/Makefile \
src/bubasics/Makefile \
src/bucoord/Makefile \
src/budb/Makefile \
src/tbudb/Makefile \
src/butc/Makefile \
src/butm/Makefile \
src/cmd/Makefile \
src/cmd/test/Makefile \
src/comerr/Makefile \
src/comerr/test/Makefile \
src/config/Makefile \
src/config/Makefile.config \
src/config/Makefile.version-CML \
src/config/Makefile.version-NOCML \
src/config/shlib-build \
src/config/shlib-install \
src/des/Makefile \
src/des/test/Makefile \
src/dir/Makefile \
src/dir/test/Makefile \
src/dviced/Makefile \
src/dvolser/Makefile \
src/export/Makefile \
src/finale/Makefile \
src/fsint/Makefile \
src/fsprobe/Makefile \
src/gtx/Makefile \
src/JAVA/libjafs/Makefile \
src/kauth/Makefile \
src/kauth/test/Makefile \
src/kopenafs/Makefile \
src/libacl/Makefile \
src/libacl/test/Makefile \
src/libadmin/Makefile \
src/libadmin/adminutil/Makefile \
src/libadmin/bos/Makefile \
src/libadmin/cfg/Makefile \
src/libadmin/cfg/test/Makefile \
src/libadmin/client/Makefile \
src/libadmin/kas/Makefile \
src/libadmin/pts/Makefile \
src/libadmin/samples/Makefile \
src/libadmin/test/Makefile \
src/libadmin/vos/Makefile \
src/libafs/Makefile.common \
src/libafs/MakefileProto.${MKAFS_OSTYPE} \
${DARWIN_PLIST} \
src/libafsauthent/Makefile \
src/libafsrpc/Makefile \
src/libuafs/Makefile.common \
src/libuafs/MakefileProto.${MKAFS_OSTYPE} \
src/log/Makefile \
src/log/test/Makefile \
src/lwp/Makefile \
src/lwp/test/Makefile \
src/packaging/Debian/changelog \
src/packaging/MacOS/OpenAFS-debug.Description.plist \
src/packaging/MacOS/OpenAFS-debug.Info.plist \
src/packaging/MacOS/OpenAFS.Description.plist \
src/packaging/MacOS/OpenAFS.Info.plist \
src/packaging/MacOS/OpenAFS.info \
src/packaging/MacOS/buildpkg.sh \
src/packaging/MacOS/pkgbuild.sh \
src/pam/Makefile \
src/platform/Makefile \
src/platform/${MKAFS_OSTYPE}/Makefile \
src/platform/DARWIN/growlagent/Makefile \
src/procmgmt/Makefile \
src/procmgmt/test/Makefile \
src/ptserver/Makefile \
src/tptserver/Makefile \
src/rx/Makefile \
src/rx/bulk.example/Makefile \
src/rx/bulktest/Makefile \
src/rx/multi.example/Makefile \
src/rx/simple.example/Makefile \
src/rx/test/Makefile \
src/rxdebug/Makefile \
src/rxgen/Makefile \
src/rxkad/Makefile \
src/rxkad/test/Makefile \
src/rxstat/Makefile \
src/scout/Makefile \
src/sgistuff/Makefile \
src/shlibafsauthent/Makefile \
src/shlibafsrpc/Makefile \
src/sys/Makefile \
src/tbutc/Makefile \
src/tests/Makefile \
src/tests/run-tests \
src/tsalvaged/Makefile \
src/tsm41/Makefile \
src/tviced/Makefile \
src/tvolser/Makefile \
src/ubik/Makefile \
src/tubik/Makefile \
src/update/Makefile \
src/usd/Makefile \
src/usd/test/Makefile \
src/uss/Makefile \
src/util/Makefile \
src/util/test/Makefile \
src/libafscp/Makefile \
src/venus/Makefile \
src/venus/test/Makefile \
src/vfsck/Makefile \
src/viced/Makefile \
src/vlserver/Makefile \
src/tvlserver/Makefile \
src/vol/Makefile \
src/vol/test/Makefile \
src/volser/Makefile \
src/xstat/Makefile \
src/helper-splint.sh \
tests/Makefile \
tests/rpctestlib/Makefile \
tests/tap/Makefile \
tests/util/Makefile,

[chmod a+x src/config/shlib-build
 chmod a+x src/config/shlib-install])

# print a final summary
SUMMARY
