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
       RRA_LIB_GSSAPI_RESTORE])
  AC_SUBST([BUILD_GSSAPI])

dnl Determine if we should build rxgk
  BUILD_RXGK=no
  AS_IF([test x"$BUILD_GSSAPI" = xyes],
    [BUILD_RXGK=yes
dnl At this point, we're not using any GSS-API bits yet, but we'll need
dnl gss_pseudo_random() in the future
     AS_IF([test x"$ac_cv_func_gss_pseudo_random" != xyes],
       [AC_MSG_NOTICE([GSS-API does not have gss_pseudo_random, this may break in the future])])])
  AC_SUBST([BUILD_RXGK])
  AS_IF([test x"$BUILD_RXGK" = xyes],
        [AC_DEFINE([BUILD_RXGK], [1], [Build rxgk])])

dnl Determine if we should enable rxgk support (note that this is a different
dnl decision than whether we should build rxgk)
  ENABLE_RXGK="no"
  RXGK_LIBS=""
  RXGK_LIBS_RPC=""
  RXGK_CFLAGS=""
  RXGK_GSSAPI_LIBS=""
  AS_IF([test "$enable_rxgk" = yes],
    [AS_IF([test "$BUILD_RXGK" = yes],
       [ENABLE_RXGK="yes"
        RXGK_LIBS="\$(top_builddir)/src/rxgk/liboafs_rxgk.la"
        RXGK_LIBS_RPC="\$(top_builddir)/src/rxgk/librxgk_pic.la"
        RXGK_CFLAGS="\$(CPPFLAGS_gssapi)"
        RXGK_GSSAPI_LIBS="\$(LDFLAGS_gssapi) \$(LIB_gssapi)"
        AC_DEFINE([ENABLE_RXGK], [1],
                  [Build rxgk support into applications])],

       [AC_MSG_ERROR([Insufficient GSS-API support to enable rxgk])])],
    [ENABLE_RXGK="no"])

  AC_SUBST([ENABLE_RXGK])
  AC_SUBST([RXGK_LIBS])
  AC_SUBST([RXGK_LIBS_RPC])
  AC_SUBST([RXGK_CFLAGS])
  AC_SUBST([RXGK_GSSAPI_LIBS])])
