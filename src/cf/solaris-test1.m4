dnl This test is for the Solaris x86 kernel module
dnl build.  They prevent newer Solaris compilers (12.3
dnl and up) from using XMM registers (SIMD instructions)
dnl and floating point registers, which are invalid in
dnl Solaris kernel code.
dnl Without this, Solaris may panic in libafs with:
dnl   BAD TRAP: type=7 (#nm Device not available)
dnl
dnl
AC_DEFUN([SOLARIS_CC_TAKES_XVECTOR_NONE], [
  AC_CACHE_CHECK([if $CC accepts -xvector=%none],
    [ac_cv_solaris_cc_takes_xvector_none],
    [save_CFLAGS="$CFLAGS"
     CFLAGS="$CFLAGS -xvector=%none"
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[int x;]])],[ac_cv_solaris_cc_takes_xvector_none=yes],[ac_cv_solaris_cc_takes_xvector_none=no])
     CFLAGS="$save_CFLAGS"
    ])

dnl -xvector=%none first appeared in Studio 11, but has only been
dnl documented as required for Solaris x86 kernel code since Studio
dnl 12.3.  Studio 12.3 is when the compiler started making more
dnl aggressive optimizations by using SSE/SIMD instructions with XMM
dnl (floating point/ SIMD) registers.  Although -xvector=%none is
dnl required to prevent these optimizations, it is not sufficient.
dnl Experiments have shown that -xregs=no%float is also needed to
dnl 1) eliminate a few optimizations not squelched by -xvector=%none,
dnl and 2) prevent actual use of floating point types in the kernel
dnl module.  -xregs=no%float has been present since before Studio 8, so
dnl it is safe to assume its presence when -xvector=%none is present.
dnl

  AS_IF([test "$ac_cv_solaris_cc_takes_xvector_none" = "yes"],
        [SOLARIS_CC_KOPTS="-xvector=%none -xregs=no%float "])
])

