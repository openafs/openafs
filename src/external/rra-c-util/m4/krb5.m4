dnl Find the compiler and linker flags for Kerberos.
dnl
dnl Finds the compiler and linker flags for linking with Kerberos libraries.
dnl Provides the --with-krb5, --with-krb5-include, and --with-krb5-lib
dnl configure options to specify non-standard paths to the Kerberos libraries.
dnl Uses krb5-config where available unless reduced dependencies is requested
dnl or --with-krb5-include or --with-krb5-lib are given.
dnl
dnl Provides the macro RRA_LIB_KRB5 and sets the substitution variables
dnl KRB5_CPPFLAGS, KRB5_LDFLAGS, and KRB5_LIBS.  Also provides
dnl RRA_LIB_KRB5_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the
dnl Kerberos libraries, saving the current values first, and
dnl RRA_LIB_KRB5_RESTORE to restore those settings to before the last
dnl RRA_LIB_KRB5_SWITCH.  HAVE_KERBEROS will always be defined if RRA_LIB_KRB5
dnl is used.
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
dnl Also provides RRA_INCLUDES_KRB5, which are the headers to include when
dnl probing the Kerberos library properties.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <rra@stanford.edu>
dnl Copyright 2005, 2006, 2007, 2008, 2009, 2010, 2011
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.

dnl Headers to include when probing for Kerberos library properties.
AC_DEFUN([RRA_INCLUDES_KRB5], [[
#if HAVE_KRB5_H
# include <krb5.h>
#else
# include <krb5/krb5.h>
#endif
]])

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the Kerberos flags.  Used as a wrapper, with
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

dnl Does the appropriate library checks for reduced-dependency Kerberos
dnl linkage.  The single argument, if true, says to fail if Kerberos could not
dnl be found.
AC_DEFUN([_RRA_LIB_KRB5_REDUCED],
[RRA_LIB_KRB5_SWITCH
 AC_CHECK_LIB([krb5], [krb5_init_context], [KRB5_LIBS="-lkrb5"],
     [AS_IF([test x"$1" = xtrue],
         [AC_MSG_ERROR([cannot find usable Kerberos library])])])
 LIBS="$KRB5_LIBS $LIBS"
 AC_CHECK_HEADERS([krb5.h krb5/krb5.h])
 AC_CHECK_FUNCS([krb5_get_error_message],
     [AC_CHECK_FUNCS([krb5_free_error_message])],
     [AC_CHECK_FUNCS([krb5_get_error_string], [],
         [AC_CHECK_FUNCS([krb5_get_err_txt], [],
             [AC_CHECK_LIB([ksvc], [krb5_svc_get_msg],
                 [KRB5_LIBS="$KRB5_LIBS -lksvc"
                  AC_DEFINE([HAVE_KRB5_SVC_GET_MSG], [1])
                  AC_CHECK_HEADERS([ibm_svc/krb5_svc.h], [], [],
                     [RRA_INCLUDES_KRB5])],
                 [AC_CHECK_LIB([com_err], [com_err],
                     [KRB5_LIBS="$KRB5_LIBS -lcom_err"],
                     [AC_MSG_ERROR([cannot find usable com_err library])])
                  AC_CHECK_HEADERS([et/com_err.h])])])])])
 RRA_LIB_KRB5_RESTORE])

dnl Does the appropriate library checks for Kerberos linkage when we don't
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
            [AC_MSG_ERROR([cannot find usable Kerberos library])])],
        [$rra_krb5_extra])],
    [-lasn1 -lcom_err -lcrypto $rra_krb5_extra])
 LIBS="$KRB5_LIBS $LIBS"
 AC_CHECK_HEADERS([krb5.h krb5/krb5.h])
 AC_CHECK_FUNCS([krb5_get_error_message],
     [AC_CHECK_FUNCS([krb5_free_error_message])],
     [AC_CHECK_FUNCS([krb5_get_error_string], [],
         [AC_CHECK_FUNCS([krb5_get_err_txt], [],
             [AC_CHECK_FUNCS([krb5_svc_get_msg],
                 [AC_CHECK_HEADERS([ibm_svc/krb5_svc.h], [], [],
                     [RRA_INCLUDES_KRB5])],
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

dnl Determine Kerberos compiler and linker flags from krb5-config.  Does the
dnl additional probing we need to do to uncover error handling features, and
dnl falls back on the manual checks.
AC_DEFUN([_RRA_LIB_KRB5_CONFIG],
[RRA_KRB5_CONFIG([${rra_krb5_root}], [krb5], [KRB5],
    [_RRA_LIB_KRB5_CHECK([$1])
     RRA_LIB_KRB5_SWITCH
     AC_CHECK_HEADERS([krb5.h krb5/krb5.h])
     AC_CHECK_FUNCS([krb5_get_error_message],
         [AC_CHECK_FUNCS([krb5_free_error_message])],
         [AC_CHECK_FUNCS([krb5_get_error_string], [],
             [AC_CHECK_FUNCS([krb5_get_err_txt], [],
                 [AC_CHECK_FUNCS([krb5_svc_get_msg],
                     [AC_CHECK_HEADERS([ibm_svc/krb5_svc.h], [], [],
                         [RRA_INCLUDES_KRB5])],
                     [AC_CHECK_HEADERS([et/com_err.h])])])])])
     RRA_LIB_KRB5_RESTORE],
    [_RRA_LIB_KRB5_PATHS
     _RRA_LIB_KRB5_MANUAL([$1])])])

dnl The core of the library checking, shared between RRA_LIB_KRB5 and
dnl RRA_LIB_KRB5_OPTIONAL.  The single argument, if "true", says to fail if
dnl Kerberos could not be found.
AC_DEFUN([_RRA_LIB_KRB5_INTERNAL],
[AC_REQUIRE([RRA_ENABLE_REDUCED_DEPENDS])
 AS_IF([test x"$rra_reduced_depends" = xtrue],
    [_RRA_LIB_KRB5_PATHS
     _RRA_LIB_KRB5_REDUCED([$1])],
    [AS_IF([test x"$rra_krb5_includedir" = x && test x"$rra_krb5_libdir" = x],
        [_RRA_LIB_KRB5_CONFIG([$1])],
        [_RRA_LIB_KRB5_PATHS
         _RRA_LIB_KRB5_MANUAL([$1])])])
 rra_krb5_uses_com_err=false
 AS_CASE([$LIBS], [*-lcom_err*], [rra_krb5_uses_com_err=true])
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
        [Location of Kerberos headers and libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_krb5_root="$withval"])])
 AC_ARG_WITH([krb5-include],
    [AS_HELP_STRING([--with-krb5-include=DIR],
        [Location of Kerberos headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_krb5_includedir="$withval"])])
 AC_ARG_WITH([krb5-lib],
    [AS_HELP_STRING([--with-krb5-lib=DIR],
        [Location of Kerberos libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_krb5_libdir="$withval"])])
 _RRA_LIB_KRB5_INTERNAL([true])
 AC_DEFINE([HAVE_KERBEROS], 1, [Define to enable Kerberos features.])])

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
        [Location of Kerberos headers and libraries])],
    [AS_IF([test x"$withval" = xno],
        [rra_use_kerberos=false],
        [AS_IF([test x"$withval" != xyes], [rra_krb5_root="$withval"])
         rra_use_kerberos=true])])
 AC_ARG_WITH([krb5-include],
    [AS_HELP_STRING([--with-krb5-include=DIR],
        [Location of Kerberos headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_krb5_includedir="$withval"])])
 AC_ARG_WITH([krb5-lib],
    [AS_HELP_STRING([--with-krb5-lib=DIR],
        [Location of Kerberos libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_krb5_libdir="$withval"])])

 AS_IF([test x"$rra_use_kerberos" != xfalse],
     [AS_IF([test x"$rra_use_kerberos" = xtrue],
         [_RRA_LIB_KRB5_INTERNAL([true])],
         [_RRA_LIB_KRB5_INTERNAL([false])])],
     [AM_CONDITIONAL([KRB5_USES_COM_ERR], [false])])
 AS_IF([test x"$KRB5_LIBS" != x],
    [AC_DEFINE([HAVE_KERBEROS], 1, [Define to enable Kerberos features.])])])

dnl Source used by RRA_FUNC_KRB5_GET_INIT_CREDS_OPT_FREE_ARGS.
AC_DEFUN([_RRA_FUNC_KRB5_OPT_FREE_ARGS_SOURCE], [RRA_INCLUDES_KRB5] [[
int
main(void)
{
    krb5_get_init_creds_opt *opts;
    krb5_context c;
    krb5_get_init_creds_opt_free(c, opts);
}
]])

dnl Check whether krb5_get_init_creds_opt_free takes one argument or two.
dnl Early Heimdal used to take a single argument.  Defines
dnl HAVE_KRB5_GET_INIT_CREDS_OPT_FREE_2_ARGS if it takes two arguments.
dnl
dnl Should be called with RRA_LIB_KRB5_SWITCH active.
AC_DEFUN([RRA_FUNC_KRB5_GET_INIT_CREDS_OPT_FREE_ARGS],
[AC_CACHE_CHECK([if krb5_get_init_creds_opt_free takes two arguments],
    [rra_cv_func_krb5_get_init_creds_opt_free_args],
    [AC_COMPILE_IFELSE([AC_LANG_SOURCE([_RRA_FUNC_KRB5_OPT_FREE_ARGS_SOURCE])],
        [rra_cv_func_krb5_get_init_creds_opt_free_args=yes],
        [rra_cv_func_krb5_get_init_creds_opt_free_args=no])])
 AS_IF([test $rra_cv_func_krb5_get_init_creds_opt_free_args = yes],
    [AC_DEFINE([HAVE_KRB5_GET_INIT_CREDS_OPT_FREE_2_ARGS], 1,
        [Define if krb5_get_init_creds_opt_free takes two arguments.])])])
