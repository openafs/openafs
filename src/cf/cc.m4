dnl OPENAFS_PATH_CC
dnl
dnl Set the CC var, to specify the C compiler to use.
dnl
dnl This is needed on some platforms where kernel modules must be built with
dnl certain compilers. For some platforms, we forcibly set CC to certain values,
dnl even if the user has given a different CC. We shouldn't do this on new
dnl platforms, but this behavior is kept for older platforms to make sure we don't
dnl change their behavior. For most modern platforms, we try to honor a CC
dnl given by the user, but provide a more helpful default CC if they didn't give
dnl one.
dnl
dnl This macro must be called early, before anything calls AC_PROG_CC directly
dnl or indirectly (many autoconf macros AC_REQUIRE([AC_PROG_CC]), because
dnl AC_PROG_CC runs some compiler checks with the detected compiler that we can't
dnl easily undo later. That means before this is called, you cannot call
dnl AC_COMPILE_IFELSE, AC_TRY_KBUILD, etc; no running the compiler.
AC_DEFUN([OPENAFS_PATH_CC], [
  AC_REQUIRE([AC_CANONICAL_HOST])

  dnl If someone called AC_PROG_CC before us, this will throw an error during
  dnl 'regen'. This isn't completely foolproof; if something in here calls
  dnl something that requires AC_PROG_CC (e.g. AC_USE_SYSTEM_EXTENSIONS), then
  dnl AC_PROG_CC will effectively called before we reach here, but this check
  dnl won't trigger. This is just the best we can do.
  AC_PROVIDE_IFELSE([AC_PROG_CC],
   [AC_FATAL([AC_PROG_CC was called before $0])])

  AS_CASE([$host],
   dnl hp_ux102
   [hppa*-hp-hpux10*],
     [CC="/opt/ansic/bin/cc -Ae"],

   dnl hp_ux11*
   [hppa*-hp-hpux11.*],
     [CC="/opt/ansic/bin/cc"],

   dnl ia64_hpux*
   [ia64-hp-hpux*],
     [CC="/opt/ansic/bin/cc"],

   dnl ppc_darwin_70
   [powerpc-apple-darwin7*],
     [CC="cc"],

   dnl *_darwin_80
   [powerpc-apple-darwin8.* | i386-apple-darwin8.*],
     [CC="cc"],

   dnl rs_aix4*
   [power*-ibm-aix4.*],
     [CC="cc"],

   dnl rs_aix5*
   [power*-ibm-aix5.*],
     [CC="cc"],

   dnl rs_aix61 | rs_aix71
   [power*-ibm-aix6.* | power*-ibm-aix7.1],
     [CC="cc"],

   dnl rs_aix7*
   [power*-ibm-aix7.*],
     [AIX7_PATH_CC],

   dnl sgi_65
   [mips-sgi-irix6.5],
     [CC="/usr/bin/cc"],

   dnl sun4x_5*
   [sparc-sun-solaris2.*],
     [SOLARIS_PATH_CC],

   dnl sunx86_5*
   [i386-pc-solaris2.*],
     [SOLARIS_PATH_CC],
   [])
])

AC_DEFUN([AIX7_PATH_CC], [
  # On AIX, we need to use the xlc compiler. Starting with AIX 7.2, a new
  # version of the compiler (17.1) is available, which is invoked via
  # 'ibm-clang'. The old compiler (16.x and below) may still be available, and
  # is invoked via 'xlc' or 'cc'. Traditionally we have invoked the old
  # compiler via 'cc', so look for that.

  # First, try to find ibm-clang in the user's PATH. If we can't find that, try
  # to find 'cc' in the user's PATH.
  AS_IF([test x"$CC" = x],
   [AC_PATH_PROGS([CC], [ibm-clang cc])])

  AS_IF([test x"$CC" = x],
   [AC_MSG_FAILURE([m4_join([ ],
     [Could not find the ibm-clang or cc compiler.],
     [Please set CC to specify the path to the compiler.])])])
])

AC_DEFUN([SOLARIS_PATH_CC], [
  # If the user specified a path with SOLARISCC, use that. We used to pick a
  # compiler based on the SOLARISCC var, so continue to preserve the behavior
  # of setting SOLARISCC.
  AS_IF([test x"$SOLARISCC" != x], [CC="$SOLARISCC"])

  AS_IF([test x"$CC" = x], [
    # If the user didn't specify a CC, try to find one in the common locations
    # for the SUNWspro-y compiler.
    AC_PATH_PROG([CC], [cc], [],
     [m4_join([:],
       [/opt/SUNWspro/bin],
       [/opt/SunStudioExpress/bin],
       [/opt/developerstudio12.6/bin],
       [/opt/developerstudio12.5/bin],
       [/opt/solarisstudio12.4/bin],
       [/opt/solarisstudio12.3/bin],
       [/opt/solstudio12.2/bin],
       [/opt/sunstudio12.1/bin])])
      AS_IF([test x"$CC" = x],
       [AC_MSG_FAILURE([m4_join([ ],
	 [Could not find the solaris cc program.],
	 [Please set CC to specify the path to the compiler.])])])
  ])
])
