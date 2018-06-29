AC_DEFUN([OPENAFS_KRB5],
dnl Probe for Kerberos.  We have a few platform-specific overrides due to
dnl weird Kerberos implementations and installation locations.
  [AS_CASE([$AFS_SYSNAME],
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
     RRA_LIB_KRB5_SWITCH
     AC_CHECK_FUNCS([add_error_table \
                     add_to_error_table \
                     encode_krb5_enc_tkt_part \
                     encode_krb5_ticket \
                     krb5_524_conv_principal \
                     krb5_allow_weak_crypto \
                     krb5_c_encrypt \
                     krb5_decode_ticket \
                     krb5_enctype_enable \
                     krb5_free_keytab_entry_contents \
                     krb5_free_unparsed_name \
                     krb5_get_init_creds_opt_alloc \
                     krb5_get_prompt_types \
                     krb5_princ_size \
                     krb5_principal_get_comp_string])
     AC_CHECK_FUNCS([krb5_524_convert_creds], [],
       [AC_CHECK_FUNCS([krb524_convert_creds_kdc], [],
          [AC_CHECK_LIB([krb524], [krb524_convert_creds_kdc],
             [LIBS="-lkrb524 $LIBS"
              KRB5_LIBS="-lkrb524 $KRB5_LIBS"
              AC_CHECK_LIB([krb524], [krb5_524_conv_principal],
                [AC_DEFINE([HAVE_KRB5_524_CONV_PRINCIPAL], [1],
                           [Define to 1 if you have the `krb5_524_conv_principal' function.])])
              AC_DEFINE([HAVE_KRB524_CONVERT_CREDS_KDC], [1],
                        [Define to 1 if you have the `krb524_convert_creds_kdc' function.])])])])
     AC_CHECK_HEADERS([kerberosIV/krb.h])
     AC_CHECK_HEADERS([kerberosV/heim_err.h])
     AC_CHECK_HEADERS([com_err.h et/com_err.h krb5/com_err.h])
     AS_IF([test x"$ac_cv_header_com_err_h" != xyes \
               && test x"$ac_cv_header_et_com_err_h" != xyes \
               && test x"$ac_cv_header_krb5_com_err_h" != xyes],
       [AC_MSG_ERROR([Cannot find a usable com_err.h])])
     AC_CHECK_MEMBERS([krb5_creds.keyblock, krb5_creds.keyblock.enctype, krb5_creds.session,
                       krb5_prompt.type], [], [], [#include <krb5.h>])
     AC_CHECK_MEMBERS([krb5_keytab_entry.key, krb5_keytab_entry.keyblock],
                      [], [], [#include <krb5.h>])
dnl If we have krb5_creds.session, we are using heimdal
dnl If we're using heimdal, aklog needs libasn1 for encode_EncTicketPart and a
dnl few other functions. But just aklog; not any of the other stuff that uses
dnl krb5.
     AS_IF([test x"$ac_cv_member_krb5_creds_session" = xyes],
           [AC_CHECK_LIB([asn1], [encode_EncTicketPart],
                         [AKLOG_KRB5_LIBS="-lasn1"])])
     RRA_LIB_KRB5_RESTORE])
  AC_SUBST([BUILD_KRB5])
  AC_SUBST([MAKE_KRB5])
  AC_SUBST([AKLOG_KRB5_LIBS])])
