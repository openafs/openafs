AC_DEFUN([OPENAFS_GSS],
dnl Probe for GSSAPI
  [RRA_LIB_GSSAPI
dnl Check for the characteristics of whatever GSSAPI we found, if we found one
  BUILD_GSSAPI=no
  AS_IF([test x"$GSSAPI_LIBS" != x],
      [BUILD_GSSAPI=yes
       RRA_LIB_GSSAPI_SWITCH
       AC_CHECK_FUNCS([gss_pseudo_random \
                       krb5_gss_register_acceptor_identity \
                       gss_krb5_ccache_name])
dnl We only need gssapi for rxgk (at this point).  rxgk requires pseudo_random.
       AS_IF([test x"$ac_cv_func_gss_pseudo_random" != xyes],
             [BUILD_GSSAPI=no])
       RRA_LIB_GSSAPI_RESTORE])
  AC_SUBST([BUILD_GSSAPI])])
