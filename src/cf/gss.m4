dnl There are a few things we define as a result of this function:
dnl
dnl - BUILD_RXGK is "#define"d if we are able to build rxgk components.
dnl
dnl - ENABLE_RXGK is "#define"d if BUILD_RXGK is defined and we are supposed to
dnl   turn on rxgk support in various binaries. (If BUILD_RXGK is defined, but
dnl   not ENABLE_RXGK, we're supposed to build rxgk components, but rxgk
dnl   functionality should be disabled in all binaries.)
dnl
dnl - @BUILD_RXGK@ is "yes" if BUILD_RXGK is defined, and "no" otherwise.
dnl
dnl - @ENABLE_RXGK@ is "yes" if ENABLE_RXGK is defined, and "no" otherwise.
dnl
dnl - @IF_RXGK@ is "" if ENABLE_RXGK is defined, and "#" otherwise. This is
dnl   helpful in makefiles to conditionally change build rules depending on if
dnl   rxgk is enabled or not. For example, this line:
dnl
dnl     @IF_RXGK@LIBS=$(top_builddir)/src/rxgk/liboafs_rxgk.la
dnl
dnl will expand to this when rxgk is disabled:
dnl
dnl     #LIBS=$(top_builddir)/src/rxgk/liboafs_rxgk.la
dnl
dnl and will expand to this when enabled:
dnl
dnl     LIBS=$(top_builddir)/src/rxgk/liboafs_rxgk.la
dnl
dnl So in other words, lines prefixed with @IF_RXGK@ will only be interpreted
dnl when rxgk support is enabled.
AC_DEFUN([OPENAFS_GSS],
  [
dnl Probe for GSSAPI
dnl
dnl Don't do this if we don't have krb5. Otherwise, if someone runs configure
dnl with no arguments on a system without krb5 libs, RRA_LIB_GSSAPI will fail,
dnl preventing the build from moving forwards.
dnl
dnl Also don't probe for GSSAPI if --without-gssapi was given, so we don't
dnl accidentally autodetect gss libs and use them.
  AS_IF([test x"$BUILD_KRB5" = xyes && test x"$with_gssapi" != xno],
	[RRA_LIB_GSSAPI])

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
  IF_RXGK="#"
  AS_IF([test "$enable_rxgk" = yes],
    [AS_IF([test "$BUILD_RXGK" = yes],
       [ENABLE_RXGK="yes"
	IF_RXGK=""
        AC_DEFINE([ENABLE_RXGK], [1],
                  [Build rxgk support into applications])],

       [AC_MSG_ERROR([Insufficient GSS-API support to enable rxgk])])],
    [ENABLE_RXGK="no"])

  AC_SUBST([ENABLE_RXGK])
  AC_SUBST([IF_RXGK])])
