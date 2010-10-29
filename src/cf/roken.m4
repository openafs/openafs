
AC_DEFUN([_OPENAFS_ROKEN_INTERNAL], [
  DIR_roken=roken
  CPPFLAGS_roken=
  LDFLAGS_roken="-L\$(TOP_LIBDIR)"
  LIB_roken=-lrokenafs
])

dnl _OPENAFS_ROKEN_CHECK($path,
dnl 			 $action-if-found,
dnl 			 $action-if-not-found)
dnl Find a roken library at $path.
dnl
dnl If $path is not specified,
dnl try to find one in the standard locations on the system.
dnl
dnl If we fail, and $path was given, then error out. Otherwise,
dnl fall back to the internal roken implementation
AC_DEFUN([_OPENAFS_ROKEN_CHECK], [
  roken_path=$1

  save_CPPFLAGS=$CPPFLAGS
  save_LDFLAGS=$LDFLAGS
  save_LIBS=$LIBS
  AS_IF([test x"$roken_path" != x],
	[CPPFLAGS="-I$roken_path/include $CPPFLAGS"
	 LDFLAGS="-L$roken_path/lib $LDFLAGS"])

  dnl Need to be careful what we check for here, as libroken contains
  dnl different symbols on different platforms.
  AC_CHECK_LIB([roken], [ct_memcmp], [found_foundlib=true])
  AC_CHECK_HEADER([roken], [roken_foundheader=true])
  CPPFLAGS=$save_CPPFLAGS
  LDFLAGS=$save_LDFLAGS

  AS_IF([test x"$roken_foundlib" = xtrue && test x"roken_foundheader" = xtrue],
	[AS_IF([test x"$roken_path" != x],
	       [CPPFLAGS_roken = "-I$roken_path/include"
		LDFLAGS_roken="-I$roken_path/lib"
		LIB_roken="-lroken"])
	 $2],
	[$3])
])

AC_DEFUN([OPENAFS_ROKEN], [
  roken_root=
  AC_SUBST(LIB_roken)
  AC_SUBST(CPPFLAGS_roken)
  AC_SUBST(LDFLAGS_roken)
  AC_SUBST(DIR_roken)

  AC_ARG_WITH([roken],
    [AS_HELP_STRING([--with-roken=DIR],
	[Location of the roken library, or 'internal'])],
    [AS_IF([test x"$withval" = xno],
	   [AC_ERROR("OpenAFS requires roken to build")],
	   [AS_IF([test x"$withval" != xyes],
	          [roken_root="$withval"])
	   ])
    ])

  AS_IF([test x"$roken_root" = xinteral],
	[_OPENAFS_ROKEN_INTERNAL()],
	[AS_IF([test x"$roken_root" = x],
	    [_OPENAFS_ROKEN_CHECK([], [], [_OPENAFS_ROKEN_INTERNAL()])],
	    [_OPENAFS_ROKEN_CHECK($roken_root,
		[AC_MSG_ERROR([Cannot find roken at that location])])
	    ])
	])
])
