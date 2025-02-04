dnl
dnl Set KINCLUDES and KROOT for building the kernel module on macos.
dnl
dnl The include path needed for building kernel extensions moved from
dnl /System/Library/Frameworks/Kernel.framework/Headers
dnl to
dnl /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/Kernel.framework/Headers
dnl sometime around darwin_180. Detect which path exists, and use the first one
dnl we find, preferring the "CommandLineTools" (clt) path, then the path from
dnl 'xcrun --show-sdk-path'.
dnl
AC_DEFUN([OPENAFS_MACOS_KINCLUDES], [

  clt_kroot=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
  xcode_kroot="$(xcrun --show-sdk-path)"
  hdr_path=/System/Library/Frameworks/Kernel.framework/Headers

  AC_MSG_CHECKING([for path to macOS kernel extension headers])
  KINCLUDES=
  for KROOT in "$clt_kroot" "$xcode_kroot" '' ; do
    AS_IF([test -e "$KROOT$hdr_path"],
     [KINCLUDES="-I$KROOT$hdr_path"
      AC_MSG_RESULT([$KINCLUDES])
      break])
  done

  AS_IF([test x"$KINCLUDES" = x],
   [AC_MSG_RESULT([none])
    AS_CASE([$enable_kernel_module],
     [yes],
       [AC_MSG_ERROR([Cannot find kernel extension headers (${hdr_path})])],
     [maybe],
       [enable_kernel_module="no"
	AC_MSG_WARN(m4_normalize([
	  Cannot find kernel extension headers (${hdr_path}), disabling
	  building the kernel module])
	)])])

  AC_SUBST([KROOT])
  AC_SUBST([KINCLUDES])
])

dnl
dnl Set ARCH_foo=yes for each arch we are building for (that is, ARCHFLAGS
dnl contains '-arch foo'). If ARCHFLAGS is not set, define the arch we are on as
dnl the default. See $possible_arches for the arches we understand.
dnl
AC_DEFUN([OPENAFS_MACOS_ARCH], [
  possible_arches="ppc i386 x86_64 armv6 armv7 arm64"

  set_arch=
  AS_IF([test x"$ARCHFLAGS" != x],
   [for arch in $possible_arches ; do
      AS_CASE(["$ARCHFLAGS"],
       [*"$arch"*],
	[set_arch=1
	 eval ARCH_"$arch"=yes])
    done])

  dnl If we didn't set any known ARCH_foo vars, figure out a default.
  AS_IF([test x"$set_arch" = x],
   [AS_CASE(["$host"],
     [powerpc-*],
       [ARCH_ppc=yes],
     [i?86-*],
       [ARCH_i386=yes],
     [x86_64-*],
       [ARCH_x86_64=yes],
     [arm-* | aarch64-*],
       [ARCH_arm64=yes],
     [AC_MSG_ERROR(m4_normalize([
	Cannot determine default arch (host $host), try building with, e.g.,
	ARCHFLAGS='-arch x86_64'])
      )])])

  AC_SUBST([ARCH_ppc])
  AC_SUBST([ARCH_i386])
  AC_SUBST([ARCH_x86_64])
  AC_SUBST([ARCH_armv6])
  AC_SUBST([ARCH_armv7])
  AC_SUBST([ARCH_arm64])

  AC_SUBST([ARCHFLAGS])
])
