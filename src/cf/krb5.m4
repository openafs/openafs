dnl Find the compiler and linker flags for Kerberos v5.
dnl
dnl Finds the compiler and linker flags for linking with Kerberos v5
dnl libraries.  Provides the --with-krb5, --with-krb5-include, and
dnl --with-krb5-lib configure options to specify non-standard paths to the
dnl Kerberos libraries.  Uses krb5-config where available unless reduced
dnl dependencies is requested.
dnl
dnl Provides the macro RRA_LIB_KRB5 and sets the substitution variables
dnl KRB5_CPPFLAGS, KRB5_LDFLAGS, and KRB5_LIBS.  Also provides
dnl RRA_LIB_KRB5_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl Kerberos libraries, saving the current values first, and
dnl RRA_LIB_KRB5_RESTORE to restore those settings to before the last
dnl RRA_LIB_KRB5_SWITCH.
dnl
dnl If KRB5_CPPFLAGS, KRB5_LDFLAGS, or KRB5_LIBS are set before calling these
dnl macros, their values will be added to whatever the macros discover.
dnl
dnl Provides the RRA_LIB_KRB5_OPTIONAL macro, which should be used if Kerberos
dnl support is optional.  This macro will still always set the substitution
dnl variables, but they'll be empty unless --with-krb5 is given.  Also,
dnl HAVE_KERBEROS will be defined if --with-krb5 is given and
dnl $rra_use_kerberos will be set to "true".
dnl
dnl Sets the Automake conditional KRB5_USES_COM_ERR saying whether we use
dnl com_err, since if we're also linking with AFS libraries, we may have to
dnl change library ordering in that case.
dnl
dnl Depends on RRA_ENABLE_REDUCED_DEPENDS and RRA_SET_LDFLAGS.
dnl
dnl Also provides RRA_FUNC_KRB5_GET_INIT_CREDS_OPT_FREE_ARGS, which checks
dnl whether krb5_get_init_creds_opt_free takes one argument or two.  Defines
dnl HAVE_KRB5_GET_INIT_CREDS_OPT_FREE_2_ARGS if it takes two arguments.
dnl
dnl Written by Russ Allbery <rra@stanford.edu>
dnl Copyright 2005, 2006, 2007, 2008, 2009, 2010
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the Kerberos v5 flags.  Used as a wrapper, with
dnl RRA_LIB_KRB5_RESTORE, around tests.
AC_DEFUN([RRA_LIB_KRB5_SWITCH],
[rra_krb5_save_CPPFLAGS="$CPPFLAGS"
 rra_krb5_save_LDFLAGS="$LDFLAGS"
 rra_krb5_save_LIBS="$LIBS"
 CPPFLAGS="$KRB5_CPPFLAGS $CPPFLAGS"
 LDFLAGS="$KRB5_LDFLAGS $LDFLAGS"
 LIBS="$KRB5_LIBS $LIBS"])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl RRA_LIB_KRB5_SWITCH was called).
AC_DEFUN([RRA_LIB_KRB5_RESTORE],
[CPPFLAGS="$rra_krb5_save_CPPFLAGS"
 LDFLAGS="$rra_krb5_save_LDFLAGS"
 LIBS="$rra_krb5_save_LIBS"])

dnl Set KRB5_CPPFLAGS and KRB5_LDFLAGS based on rra_krb5_root,
dnl rra_krb5_libdir, and rra_krb5_includedir.
AC_DEFUN([_RRA_LIB_KRB5_PATHS],
[AS_IF([test x"$rra_krb5_libdir" != x],
    [KRB5_LDFLAGS="-L$rra_krb5_libdir"],
    [AS_IF([test x"$rra_krb5_root" != x],
        [RRA_SET_LDFLAGS([KRB5_LDFLAGS], [$rra_krb5_root])])])
 AS_IF([test x"$rra_krb5_includedir" != x],
    [KRB5_CPPFLAGS="-I$rra_krb5_includedir"],
    [AS_IF([test x"$rra_krb5_root" != x],
        [AS_IF([test x"$rra_krb5_root" != x/usr],
            [KRB5_CPPFLAGS="-I${rra_krb5_root}/include"])])])])

dnl Does the appropriate library checks for reduced-dependency Kerberos v5
dnl linkage.  The single argument, if true, says to fail if Kerberos could not
dnl be found.
AC_DEFUN([_RRA_LIB_KRB5_REDUCED],
[RRA_LIB_KRB5_SWITCH
 AC_CHECK_LIB([krb5], [krb5_init_context], [KRB5_LIBS="-lkrb5"],
     [AS_IF([test x"$1" = xtrue],
         [AC_MSG_ERROR([cannot find usable Kerberos v5 library])])])
 LIBS="$KRB5_LIBS $LIBS"
 AC_CHECK_FUNCS([krb5_get_error_message],
     [AC_CHECK_FUNCS([krb5_free_error_message])],
     [AC_CHECK_FUNCS([krb5_get_error_string], [],
         [AC_CHECK_FUNCS([krb5_get_err_txt], [],
             [AC_CHECK_LIB([ksvc], [krb5_svc_get_msg],
                 [KRB5_LIBS="$KRB5_LIBS -lksvc"
                  AC_DEFINE([HAVE_KRB5_SVC_GET_MSG], [1])
                  AC_CHECK_HEADERS([ibm_svc/krb5_svc.h], [], [],
                     [#include <krb5.h>])],
                 [AC_CHECK_LIB([com_err], [com_err],
                     [KRB5_LIBS="$KRB5_LIBS -lcom_err"],
                     [AC_MSG_ERROR([cannot find usable com_err library])])
                  AC_CHECK_HEADERS([et/com_err.h])])])])])
 RRA_LIB_KRB5_RESTORE])

dnl Does the appropriate library checks for Kerberos v5 linkage when we don't
dnl have krb5-config or reduced dependencies.  The single argument, if true,
dnl says to fail if Kerberos could not be found.
AC_DEFUN([_RRA_LIB_KRB5_MANUAL],
[RRA_LIB_KRB5_SWITCH
 rra_krb5_extra=
 LIBS=
 AC_SEARCH_LIBS([res_search], [resolv], [],
    [AC_SEARCH_LIBS([__res_search], [resolv])])
 AC_SEARCH_LIBS([gethostbyname], [nsl])
 AC_SEARCH_LIBS([socket], [socket], [],
    [AC_CHECK_LIB([nsl], [socket], [LIBS="-lnsl -lsocket $LIBS"], [],
        [-lsocket])])
 AC_SEARCH_LIBS([crypt], [crypt])
 AC_SEARCH_LIBS([rk_simple_execve], [roken])
 rra_krb5_extra="$LIBS"
 LIBS="$rra_krb5_save_LIBS"
 AC_CHECK_LIB([krb5], [krb5_init_context],
    [KRB5_LIBS="-lkrb5 -lasn1 -lcom_err -lcrypto $rra_krb5_extra"],
    [AC_CHECK_LIB([krb5support], [krb5int_getspecific],
        [rra_krb5_extra="-lkrb5support $rra_krb5_extra"],
        [AC_CHECK_LIB([pthreads], [pthread_setspecific],
            [rra_krb5_pthread="-lpthreads"],
            [AC_CHECK_LIB([pthread], [pthread_setspecific],
                [rra_krb5_pthread="-lpthread"])])
         AC_CHECK_LIB([krb5support], [krb5int_setspecific],
            [rra_krb5_extra="-lkrb5support $rra_krb5_extra $rra_krb5_pthread"],
            [], [$rra_krb5_pthread $rra_krb5_extra])],
        [$rra_krb5_extra])
     AC_CHECK_LIB([com_err], [error_message],
        [rra_krb5_extra="-lcom_err $rra_krb5_extra"], [], [$rra_krb5_extra])
     AC_CHECK_LIB([ksvc], [krb5_svc_get_msg],
        [rra_krb5_extra="-lksvc $rra_krb5_extra"], [], [$rra_krb5_extra])
     AC_CHECK_LIB([k5crypto], [krb5int_hash_md5],
        [rra_krb5_extra="-lk5crypto $rra_krb5_extra"], [], [$rra_krb5_extra])
     AC_CHECK_LIB([k5profile], [profile_get_values],
        [rra_krb5_extra="-lk5profile $rra_krb5_extra"], [], [$rra_krb5_extra])
     AC_CHECK_LIB([krb5], [krb5_cc_default],
        [KRB5_LIBS="-lkrb5 $rra_krb5_extra"],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable Kerberos v5 library])])],
        [$rra_krb5_extra])],
    [-lasn1 -lcom_err -lcrypto $rra_krb5_extra])
 LIBS="$KRB5_LIBS $LIBS"
 AC_CHECK_FUNCS([krb5_get_error_message],
     [AC_CHECK_FUNCS([krb5_free_error_message])],
     [AC_CHECK_FUNCS([krb5_get_error_string], [],
         [AC_CHECK_FUNCS([krb5_get_err_txt], [],
             [AC_CHECK_FUNCS([krb5_svc_get_msg],
                 [AC_CHECK_HEADERS([ibm_svc/krb5_svc.h], [], [],
                     [#include <krb5.h>])],
                 [AC_CHECK_HEADERS([et/com_err.h])])])])])
 RRA_LIB_KRB5_RESTORE])

dnl Sanity-check the results of krb5-config and be sure we can really link a
dnl Kerberos program.  If that fails, clear KRB5_CPPFLAGS and KRB5_LIBS so
dnl that we know we don't have usable flags and fall back on the manual
dnl check.
AC_DEFUN([_RRA_LIB_KRB5_CHECK],
[RRA_LIB_KRB5_SWITCH
 AC_CHECK_FUNC([krb5_init_context],
    [RRA_LIB_KRB5_RESTORE],
    [RRA_LIB_KRB5_RESTORE
     KRB5_CPPFLAGS=
     KRB5_LIBS=
     _RRA_LIB_KRB5_PATHS
     _RRA_LIB_KRB5_MANUAL([$1])])])

dnl The core of the library checking, shared between RRA_LIB_KRB5 and
dnl RRA_LIB_KRB5_OPTIONAL.  The single argument, if "true", says to fail if
dnl Kerberos could not be found.
AC_DEFUN([_RRA_LIB_KRB5_INTERNAL],
[AC_REQUIRE([RRA_ENABLE_REDUCED_DEPENDS])
 AS_IF([test x"$rra_reduced_depends" = xtrue],
    [_RRA_LIB_KRB5_PATHS
     _RRA_LIB_KRB5_REDUCED([$1])],
    [AC_ARG_VAR([KRB5_CONFIG], [Path to krb5-config])
     AS_IF([test x"$rra_krb5_root" != x && test -z "$KRB5_CONFIG"],
         [AS_IF([test -x "${rra_krb5_root}/bin/krb5-config"],
             [KRB5_CONFIG="${rra_krb5_root}/bin/krb5-config"])],
         [AC_PATH_PROG([KRB5_CONFIG], [krb5-config], [],
             [${PATH}:/usr/kerberos/bin])])
     AS_IF([test x"$KRB5_CONFIG" != x && test -x "$KRB5_CONFIG"],
         [AC_CACHE_CHECK([for krb5 support in krb5-config],
             [rra_cv_lib_krb5_config],
             [AS_IF(["$KRB5_CONFIG" 2>&1 | grep krb5 >/dev/null 2>&1],
                 [rra_cv_lib_krb5_config=yes],
                 [rra_cv_lib_krb5_config=no])])
          AS_IF([test x"$rra_cv_lib_krb5_config" = xyes],
              [KRB5_CPPFLAGS=`"$KRB5_CONFIG" --cflags krb5 2>/dev/null`
               KRB5_LIBS=`"$KRB5_CONFIG" --libs krb5 2>/dev/null`],
              [KRB5_CPPFLAGS=`"$KRB5_CONFIG" --cflags 2>/dev/null`
               KRB5_LIBS=`"$KRB5_CONFIG" --libs 2>/dev/null`])
          KRB5_CPPFLAGS=`echo "$KRB5_CPPFLAGS" | sed 's%-I/usr/include ?%%'`
          _RRA_LIB_KRB5_CHECK([$1])
          RRA_LIB_KRB5_SWITCH
          AC_CHECK_FUNCS([krb5_get_error_message],
              [AC_CHECK_FUNCS([krb5_free_error_message])],
              [AC_CHECK_FUNCS([krb5_get_error_string], [],
                  [AC_CHECK_FUNCS([krb5_get_err_txt], [],
                      [AC_CHECK_FUNCS([krb5_svc_get_msg],
                          [AC_CHECK_HEADERS([ibm_svc/krb5_svc.h], [], [],
                              [#include <krb5.h>])],
                          [AC_CHECK_HEADERS([et/com_err.h])])])])])
          RRA_LIB_KRB5_RESTORE],
         [_RRA_LIB_KRB5_PATHS
          _RRA_LIB_KRB5_MANUAL([$1])])])
 rra_krb5_uses_com_err=false
 case "$LIBS" in
 *-lcom_err*)
     rra_krb5_uses_com_err=true
     ;;
 esac
 AM_CONDITIONAL([KRB5_USES_COM_ERR], [test x"$rra_krb5_uses_com_err" = xtrue])])

dnl The main macro for packages with mandatory Kerberos support.
AC_DEFUN([RRA_LIB_KRB5],
[rra_krb5_root=
 rra_krb5_libdir=
 rra_krb5_includedir=
 rra_use_kerberos=true
 AC_SUBST([KRB5_CPPFLAGS])
 AC_SUBST([KRB5_LDFLAGS])
 AC_SUBST([KRB5_LIBS])

 AC_ARG_WITH([krb5],
    [AS_HELP_STRING([--with-krb5=DIR],
        [Location of Kerberos v5 headers and libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_krb5_root="$withval"])])
 AC_ARG_WITH([krb5-include],
    [AS_HELP_STRING([--with-krb5-include=DIR],
        [Location of Kerberos v5 headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_krb5_includedir="$withval"])])
 AC_ARG_WITH([krb5-lib],
    [AS_HELP_STRING([--with-krb5-lib=DIR],
        [Location of Kerberos v5 libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_krb5_libdir="$withval"])])
 _RRA_LIB_KRB5_INTERNAL([true])])

dnl The main macro for packages with optional Kerberos support.
AC_DEFUN([RRA_LIB_KRB5_OPTIONAL],
[rra_krb5_root=
 rra_krb5_libdir=
 rra_krb5_includedir=
 rra_use_kerberos=
 AC_SUBST([KRB5_CPPFLAGS])
 AC_SUBST([KRB5_LDFLAGS])
 AC_SUBST([KRB5_LIBS])

 AC_ARG_WITH([krb5],
    [AS_HELP_STRING([--with-krb5@<:@=DIR@:>@],
        [Location of Kerberos v5 headers and libraries])],
    [AS_IF([test x"$withval" = xno],
        [rra_use_kerberos=false],
        [AS_IF([test x"$withval" != xyes], [rra_krb5_root="$withval"])
         rra_use_kerberos=true])])
 AC_ARG_WITH([krb5-include],
    [AS_HELP_STRING([--with-krb5-include=DIR],
        [Location of Kerberos v5 headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_krb5_includedir="$withval"])])
 AC_ARG_WITH([krb5-lib],
    [AS_HELP_STRING([--with-krb5-lib=DIR],
        [Location of Kerberos v5 libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_krb5_libdir="$withval"])])

 AS_IF([test x"$rra_use_kerberos" != xfalse],
     [AS_IF([test x"$rra_use_kerberos" = xtrue],
         [_RRA_LIB_KRB5_INTERNAL([true])],
         [_RRA_LIB_KRB5_INTERNAL([false])])],
     [AM_CONDITIONAL([KRB5_USES_COM_ERR], [false])])
 AS_IF([test x"$KRB5_LIBS" != x],
    [AC_DEFINE([HAVE_KERBEROS], 1, [Define to enable Kerberos features.])])])

dnl Check whether krb5_get_init_creds_opt_free takes one argument or two.
dnl Early Heimdal used to take a single argument.  Defines
dnl HAVE_KRB5_GET_INIT_CREDS_OPT_FREE_2_ARGS if it takes two arguments.
dnl
dnl Should be called with RRA_LIB_KRB5_SWITCH active.
AC_DEFUN([RRA_FUNC_KRB5_GET_INIT_CREDS_OPT_FREE_ARGS],
[AC_CACHE_CHECK([if krb5_get_init_creds_opt_free takes two arguments],
    [rra_cv_func_krb5_get_init_creds_opt_free_args],
    [AC_TRY_COMPILE([#include <krb5.h>],
        [krb5_get_init_creds_opt *opts; krb5_context c;
         krb5_get_init_creds_opt_free(c, opts);],
        [rra_cv_func_krb5_get_init_creds_opt_free_args=yes],
        [rra_cv_func_krb5_get_init_creds_opt_free_args=no])])
 AS_IF([test $rra_cv_func_krb5_get_init_creds_opt_free_args = yes],
    [AC_DEFINE([HAVE_KRB5_GET_INIT_CREDS_OPT_FREE_2_ARGS], 1,
        [Define if krb5_get_init_creds_opt_free takes two arguments.])])])
