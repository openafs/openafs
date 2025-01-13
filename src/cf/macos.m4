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
