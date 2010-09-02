dnl Find the compiler and linker flags for GSS-API.
dnl
dnl Finds the compiler and linker flags for linking with GSS-API libraries.
dnl Provides the --with-gssapi, --with-gssapi-include, and --with-gssapi-lib
dnl configure option to specify a non-standard path to the GSS-API libraries.
dnl Uses krb5-config where available unless reduced dependencies is requested.
dnl
dnl Provides the macro RRA_LIB_GSSAPI and sets the substitution variables
dnl GSSAPI_CPPFLAGS, GSSAPI_LDFLAGS, and GSSAPI_LIBS.  Also provides
dnl RRA_LIB_GSSAPI_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl GSS-API libraries, saving the ecurrent values, and RRA_LIB_GSSAPI_RESTORE
dnl to restore those settings to before the last RRA_LIB_GSSAPI_SWITCH.
dnl
dnl Depends on RRA_ENABLE_REDUCED_DEPENDS and RRA_SET_LDFLAGS.
dnl
dnl Written by Russ Allbery <rra@stanford.edu>
dnl Copyright 2005, 2006, 2007, 2008, 2009
dnl     Board of Trustees, Leland Stanford Jr. University
dnl
dnl See LICENSE for licensing terms.

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the GSS-API flags.  Used as a wrapper, with
dnl RRA_LIB_GSSAPI_RESTORE, around tests.
AC_DEFUN([RRA_LIB_GSSAPI_SWITCH],
[rra_gssapi_save_CPPFLAGS="$CPPFLAGS"
 rra_gssapi_save_LDFLAGS="$LDFLAGS"
 rra_gssapi_save_LIBS="$LIBS"
 CPPFLAGS="$GSSAPI_CPPFLAGS $CPPFLAGS"
 LDFLAGS="$GSSAPI_LDFLAGS $LDFLAGS"
 LIBS="$GSSAPI_LIBS $LIBS"])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl RRA_LIB_GSSAPI_SWITCH was called).
AC_DEFUN([RRA_LIB_GSSAPI_RESTORE],
[CPPFLAGS="$rra_gssapi_save_CPPFLAGS"
 LDFLAGS="$rra_gssapi_save_LDFLAGS"
 LIBS="$rra_gssapi_save_LIBS"])

dnl Set GSSAPI_CPPFLAGS and GSSAPI_LDFLAGS based on rra_gssapi_root,
dnl rra_gssapi_libdir, and rra_gssapi_includedir.
AC_DEFUN([_RRA_LIB_GSSAPI_PATHS],
[AS_IF([test x"$rra_gssapi_libdir" != x],
    [GSSAPI_LDFLAGS="-L$rra_gssapi_libdir"],
    [AS_IF([test x"$rra_gssapi_root" != x],
        [RRA_SET_LDFLAGS([GSSAPI_LDFLAGS], [$rra_gssapi_root])])])
 AS_IF([test x"$rra_gssapi_includedir" != x],
    [GSSAPI_CPPFLAGS="-I$rra_gssapi_includedir"],
    [AS_IF([test x"$rra_gssapi_root" != x],
        [AS_IF([test x"$rra_gssapi_root" != x/usr],
            [GSSAPI_CPPFLAGS="-I${rra_gssapi_root}/include"])])])])

dnl Does the appropriate library checks for reduced-dependency GSS-API
dnl linkage.
AC_DEFUN([_RRA_LIB_GSSAPI_REDUCED],
[RRA_LIB_GSSAPI_SWITCH
 AC_CHECK_LIB([gssapi_krb5], [gss_import_name], [GSSAPI_LIBS="-lgssapi_krb5"],
     [AC_CHECK_LIB([gssapi], [gss_import_name], [GSSAPI_LIBS="-lgssapi"],
         [AC_CHECK_LIB([gss], [gss_import_name], [GSSAPI_LIBS="-lgss"],
             [AC_MSG_ERROR([cannot find usable GSS-API library])])])])
 RRA_LIB_GSSAPI_RESTORE])

dnl Does the appropriate library checks for GSS-API linkage when we don't
dnl have krb5-config or reduced dependencies.  libgss is used as a last
dnl resort, since it may be a non-functional mech-independent wrapper, but
dnl it's the right choice on Solaris 10.
AC_DEFUN([_RRA_LIB_GSSAPI_MANUAL],
[RRA_LIB_GSSAPI_SWITCH
 rra_gssapi_extra=
 LIBS=
 AC_SEARCH_LIBS([res_search], [resolv], ,
    [AC_SEARCH_LIBS([__res_search], [resolv])])
 AC_SEARCH_LIBS([gethostbyname], [nsl])
 AC_SEARCH_LIBS([socket], [socket], ,
    [AC_CHECK_LIB([nsl], [socket], [LIBS="-lnsl -lsocket $LIBS"], ,
        [-lsocket])])
 AC_SEARCH_LIBS([crypt], [crypt])
 rra_gssapi_extra="$LIBS"
 LIBS="$rra_gssapi_save_LIBS"
 AC_CHECK_LIB([gssapi], [gss_import_name],
    [GSSAPI_LIBS="-lgssapi -lkrb5 -lasn1 -lroken -lcrypto -lcom_err"
     GSSAPI_LIBS="$GSSAPI_LIBS $rra_gssapi_extra"],
    [AC_CHECK_LIB([krb5support], [krb5int_getspecific],
        [rra_gssapi_extra="-lkrb5support $rra_gssapi_extra"],
        [AC_CHECK_LIB([pthreads], [pthread_setspecific],
            [rra_gssapi_pthread="-lpthreads"],
            [AC_CHECK_LIB([pthread], [pthread_setspecific],
                [rra_gssapi_pthread="-lpthread"])])
         AC_CHECK_LIB([krb5support], [krb5int_setspecific],
            [rra_gssapi_extra="-lkrb5support $rra_gssapi_extra"
             rra_gssapi_extra="$rra_gssapi_extra $rra_gssapi_pthread"], ,
            [$rra_gssapi_pthread])])
     AC_CHECK_LIB([com_err], [error_message],
        [rra_gssapi_extra="-lcom_err $rra_gssapi_extra"])
     AC_CHECK_LIB([k5crypto], [krb5int_hash_md5],
        [rra_gssapi_extra="-lk5crypto $rra_gssapi_extra"])
     rra_gssapi_extra="-lkrb5 $rra_gssapi_extra"
     AC_CHECK_LIB([gssapi_krb5], [gss_import_name],
        [GSSAPI_LIBS="-lgssapi_krb5 $rra_gssapi_extra"],
        [AC_CHECK_LIB([gss], [gss_import_name],
            [GSSAPI_LIBS="-lgss"],
            [AC_MSG_ERROR([cannot find usable GSS-API library])])],
        [$rra_gssapi_extra])],
    [-lkrb5 -lasn1 -lroken -lcrypto -lcom_err $rra_gssapi_extra])
 RRA_LIB_GSSAPI_RESTORE])

dnl Sanity-check the results of krb5-config and be sure we can really link a
dnl GSS-API program.  If not, fall back on the manual check.
AC_DEFUN([_RRA_LIB_GSSAPI_CHECK],
[RRA_LIB_GSSAPI_SWITCH
 AC_CHECK_FUNC([gss_import_name],
    [RRA_LIB_GSSAPI_RESTORE],
    [RRA_LIB_GSSAPI_RESTORE
     GSSAPI_CPPFLAGS=
     GSSAPI_LIBS=
     _RRA_LIB_GSSAPI_PATHS
     _RRA_LIB_GSSAPI_MANUAL])])

dnl The main macro.
AC_DEFUN([RRA_LIB_GSSAPI],
[AC_REQUIRE([RRA_ENABLE_REDUCED_DEPENDS])
 rra_gssapi_root=
 rra_gssapi_libdir=
 rra_gssapi_includedir=
 GSSAPI_CPPFLAGS=
 GSSAPI_LDFLAGS=
 GSSAPI_LIBS=
 AC_SUBST([GSSAPI_CPPFLAGS])
 AC_SUBST([GSSAPI_LDFLAGS])
 AC_SUBST([GSSAPI_LIBS])

 AC_ARG_WITH([gssapi],
    [AS_HELP_STRING([--with-gssapi=DIR],
        [Location of GSS-API headers and libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_gssapi_root="$withval"])])
 AC_ARG_WITH([gssapi-include],
    [AS_HELP_STRING([--with-gssapi-include=DIR],
        [Location of GSS-API headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_gssapi_includedir="$withval"])])
 AC_ARG_WITH([gssapi-lib],
    [AS_HELP_STRING([--with-gssapi-lib=DIR],
        [Location of GSS-API libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_gssapi_libdir="$withval"])])

 AS_IF([test x"$rra_reduced_depends" = xtrue],
    [_RRA_LIB_GSSAPI_PATHS
     _RRA_LIB_GSSAPI_REDUCED],
    [AC_ARG_VAR([KRB5_CONFIG], [Path to krb5-config])
     AS_IF([test x"$rra_gssapi_root" != x && test -z "$KRB5_CONFIG"],
         [AS_IF([test -x "${rra_gssapi_root}/bin/krb5-config"],
             [KRB5_CONFIG="${rra_gssapi_root}/bin/krb5-config"])],
         [AC_PATH_PROG([KRB5_CONFIG], [krb5-config])])
     AS_IF([test x"$KRB5_CONFIG" != x && test -x "$KRB5_CONFIG"],
         [AC_CACHE_CHECK([for gssapi support in krb5-config],
             [rra_cv_lib_gssapi_config],
             [AS_IF(["$KRB5_CONFIG" 2>&1 | grep gssapi >/dev/null 2>&1],
                 [rra_cv_lib_gssapi_config=yes],
                 [rra_cv_lib_gssapi_config=no])])
          AS_IF([test "$rra_cv_lib_gssapi_config" = yes],
              [GSSAPI_CPPFLAGS=`"$KRB5_CONFIG" --cflags gssapi 2>/dev/null`
               GSSAPI_LIBS=`"$KRB5_CONFIG" --libs gssapi 2>/dev/null`],
              [GSSAPI_CPPFLAGS=`"$KRB5_CONFIG" --cflags 2>/dev/null`
               GSSAPI_LIBS=`"$KRB5_CONFIG" --libs 2>/dev/null`])
          GSSAPI_CPPFLAGS=`echo "$GSSAPI_CPPFLAGS" \
              | sed 's%-I/usr/include ?%%'`
          _RRA_LIB_GSSAPI_CHECK],
         [_RRA_LIB_GSSAPI_PATHS
          _RRA_LIB_GSSAPI_MANUAL])])])
