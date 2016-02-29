dnl Run this if we are using the bundled hcrypto for everything
AC_DEFUN([_OPENAFS_HCRYPTO_INTERNAL], [
  CPPFLAGS_hcrypto=
  LDFLAGS_hcrypto="-L\$(TOP_LIBDIR)"
  LIB_hcrypto="-lafshcrypto"
  hcrypto_all_target="all-internal"
  hcrypto_install_target="install-internal"
])

dnl _OPENAFS_HCRYPTO_PATHS()
dnl Set LDFLAGS_hcrypto and LIB_hcrypto based on the values of hcrypto_root,
dnl hcrypto_libdir, and hcrypto_includedir
AC_DEFUN([_OPENAFS_HCRYPTO_PATHS], [
  AS_IF([test x"$hcrypto_libdir" != x],
    [LDFLAGS_hcrypto="-L$hcrypto_libdir"],
    [AS_IF([test x"$hcrypto_root" != x],
      [LDFLAGS_hcrypto="-L$hcrypto_root/lib"])])
  AS_IF([test x"$hcrypto_includedir" != x],
    [CPPFLAGS_hcrypto="-I$hcrypto_includedir -I$hcrypto_includedir/hcrypto"],
    [AS_IF([test x"$hcrypto_root" != x],
      [CPPFLAGS_hcrypto="-I$hcrypto_root/include -I$hcrypto_root/include/hcrypto"])])
  LIB_hcrypto="-lhcrypto"
  hcrypto_all_target="all-lwp"
  hcrypto_install_target=
  ]
)

dnl _OPENAFS_HCRYPTO_CHECK($action-if-found,
dnl                        $action-if-not-found)
dnl Find an hcrypto library using $hcrypto_root, $hcrypto_libdir, and
dnl $hcrypto_includedir (global variables)
dnl
dnl If no paths were given and no usable hcrypto is found in the standard
dnl search paths, fall back to the built-in one.  Otherwise, if no usable
dnl hcrypto is found, bail out.
AC_DEFUN([_OPENAFS_HCRYPTO_CHECK], [

  _OPENAFS_HCRYPTO_PATHS()
  save_CPPFLAGS=$CPPFLAGS
  save_LDFLAGS=$LDFLAGS
  save_LIBS=$LIBS
  AS_IF([test x"$CPPFLAGS_hcrypto" != x],
    [CPPFLAGS="$CPPFLAGS_hcrypto $CPPFLAGS"])
  AS_IF([test x"$LDFLAGS_hcrypto" != x],
    [LDFLAGS="$LDFLAGS_hcrypto $LDFLAGS"])
  AS_IF([test x"$LIB_hcrypto" != x],
    [LIBS="$LIB_hcrypto $LIBS"])
  AS_IF([test x"$hcrypto_libdir" != x || test x"$hcrypto_includedir" != x],
    [checkstr=" with specified include and lib paths"],
    [AS_IF([test x"$hcrypto_root" != x],
      [checkstr=" in $hcrypto_root"])])

  AC_MSG_CHECKING([for usable system libhcrypto$checkstr])

  dnl Could probably be more clever about what to check for here, but
  dnl what we need from hcrypto should be pretty stable.
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
      [[#include <sys/types.h>
#include <stdio.h>
#include <evp.h>
#include <hmac.h>]],
      [[EVP_aes_256_cbc();
HMAC_Init_ex(NULL, NULL, 0, NULL, NULL);
RAND_bytes(NULL, 0);]])],
    [hcrypto_found=true
    AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])])

  CPPFLAGS=$save_CPPFLAGS
  LDFLAGS=$save_LDFLAGS
  LIBS=$save_LIBS

  AS_IF([test x"$hcrypto_found" = xtrue],
    [$1], [$2])
])

AC_DEFUN([OPENAFS_HCRYPTO], [
  AC_SUBST(CPPFLAGS_hcrypto)
  AC_SUBST(LDFLAGS_hcrypto)
  AC_SUBST(LIB_hcrypto)
  AC_SUBST(hcrypto_all_target)
  AC_SUBST(hcrypto_install_target)

  AC_ARG_WITH([hcrypto],
    [AS_HELP_STRING([--with-hcrypto=DIR],[Location of the hcrypto library, or 'internal'])],
    [AS_IF([test x"$withval" = xno],
      [AC_MSG_ERROR("OpenAFS requires hcrypto to build")],
      [AS_IF([test x"$withval" != xyes],
	[hcrypto_root="$withval"])])]
  )
  AC_ARG_WITH([hcrypto-include],
    [AS_HELP_STRING([--with-hcrypto-include=DIR],[Location of hcrypto headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
      [hcrypto_includedir=$withval])])
  AC_ARG_WITH([hcrypto-lib],
    [AS_HELP_STRING([--with-hcrypto-lib=DIR],[Location of the hcrypto library])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
      [hcrypto_libdir=$withval])])

  AS_IF([test x"$hcrypto_root" = xinternal],
    [_OPENAFS_HCRYPTO_INTERNAL()],
    [AS_IF([test x"$hcrypto_root" = x && test x"$hcrypto_libdir" = x &&
	test x"$hcrypto_includedir" = x],
      [_OPENAFS_HCRYPTO_CHECK([], [_OPENAFS_HCRYPTO_INTERNAL])],
      [_OPENAFS_HCRYPTO_CHECK([],
	[AC_MSG_ERROR([Cannot find hcrypto at that location])])]
      )]
  )

])
