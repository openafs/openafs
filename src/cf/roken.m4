
AC_DEFUN([_OPENAFS_ROKEN_INTERNAL], [
  DIR_roken=roken
  CPPFLAGS_roken=
  LDFLAGS_roken="-L\$(TOP_LIBDIR)"
  LIB_roken=-lrokenafs
  buildtool_roken="\$(TOP_OBJDIR)/src/roken/librokenafs.a"
])

dnl _OPENAFS_ROKEN_PATHS()
dnl Set CPPFLAGS_roken, LDFLAGS_roken, and LIB_roken based on the values
dnl of roken_root, roken_libdir, and roken_includedir.
AC_DEFUN([_OPENAFS_ROKEN_PATHS], [
  AS_IF([test x"$roken_libdir" != x],
    [LDFLAGS_roken="-L$roken_libdir"],
    [AS_IF([test x"$roken_root" != x],
      [LDFLAGS_roken="-L$roken_root/lib"])])
  AS_IF([test x"$roken_includedir" != x],
    [CPPFLAGS_roken="-I$roken_includedir"],
    [AS_IF([test x"$roken_root" != x],
      [CPPFLAGS_roken="-I$roken_root/include"])])
  LIB_roken="-lroken"
  buildtool_roken="\$(LDFLAGS_roken) \$(LIB_roken)"])

dnl _OPENAFS_ROKEN_CHECK($action-if-found,
dnl 			 $action-if-not-found)
dnl Find a roken library using $roken_root, $roken_libdir, and $roken_includedir
dnl
dnl If none of the three paths are specified,
dnl try to find one in the standard locations on the system.
dnl
dnl If we fail, and at least one path was given, then error out. Otherwise,
dnl fall back to the internal roken implementation.
AC_DEFUN([_OPENAFS_ROKEN_CHECK], [

  _OPENAFS_ROKEN_PATHS
  save_CPPFLAGS=$CPPFLAGS
  save_LDFLAGS=$LDFLAGS
  save_LIBS=$LIBS
  AS_IF([test x"$CPPFLAGS_roken" != x],
	[CPPFLAGS="$CPPFLAGS_roken $CPPFLAGS"])
  AS_IF([test x"$LDFLAGS_roken" != x],
	[LDFLAGS="$LDFLAGS_roken $LDFLAGS"])
  AS_IF([test x"$roken_libdir" != x || test x"$roken_includedir" != x],
	[checkstr=" with specified include and lib paths"],
	[AS_IF([test x"$roken_root" != x],
		[checkstr=" in $roken_root"])])

  AC_MSG_CHECKING([for usable system libroken$checkstr])

  LIBS="$LIBS $LIB_roken"
  dnl Need to be careful what we check for here, as libroken contains
  dnl different symbols on different platforms. We cannot simply check
  dnl if e.g. rk_rename is a symbol or not, since on most platforms it
  dnl will be a preprocessor define, but on others it will be a symbol.
  dnl
  dnl Also note that we need to check for the specific functionality in
  dnl roken that we use, not just the existence of the library itself,
  dnl since older versions of roken do not contain all of the functions
  dnl we need. It may not be practical to check everything we use, so
  dnl just add functions to check here as we find installations where
  dnl this breaks.
  AC_LINK_IFELSE(
   [AC_LANG_PROGRAM(
    [[#include <roken.h>]],
    [[ct_memcmp(NULL, NULL, 0); rk_rename(NULL, NULL);]])],
   [roken_found=true
    AC_MSG_RESULT([yes])],
   [AC_MSG_RESULT([no])])

  CPPFLAGS=$save_CPPFLAGS
  LDFLAGS=$save_LDFLAGS
  LIBS=$save_LIBS

  AS_IF([test x"$roken_found" = xtrue],
	 [$1], [$2])
])

AC_DEFUN([OPENAFS_ROKEN], [
  roken_root=
  AC_SUBST(LIB_roken)
  AC_SUBST(CPPFLAGS_roken)
  AC_SUBST(LDFLAGS_roken)
  AC_SUBST(DIR_roken)
  AC_SUBST(buildtool_roken)

  AC_ARG_WITH([roken],
    [AS_HELP_STRING([--with-roken=DIR],
	[Location of the roken library, or 'internal'])],
    [AS_IF([test x"$withval" = xno],
	   [AC_ERROR("OpenAFS requires roken to build")],
	   [AS_IF([test x"$withval" != xyes],
	          [roken_root="$withval"])
	   ])
    ])
  AC_ARG_WITH([roken-include],
    [AS_HELP_STRING([--with-roken-include=DIR],
	[Location of roken headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
	[roken_includedir="$withval"])])
  AC_ARG_WITH([roken-lib],
    [AS_HELP_STRING([--with-roken-lib=DIR],
	[Location of roken libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
	[roken_libdir="$withval"])])

  AS_IF([test x"$roken_root" = xinternal],
	[_OPENAFS_ROKEN_INTERNAL()],
	[AS_IF([test x"$roken_root" = x && test x"$roken_libdir" = x &&
		test x"$roken_includedir" = x],
	    [_OPENAFS_ROKEN_CHECK([], [_OPENAFS_ROKEN_INTERNAL()])],
	    [_OPENAFS_ROKEN_CHECK([],
		[AC_MSG_ERROR([Cannot find roken at that location])])
	    ])
	])
])
