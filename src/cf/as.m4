dnl Checks related to 'as', the assembler
dnl
AC_DEFUN([OPENAFS_AS_CHECKS], [

AS_CASE([$AFS_SYSNAME],
 [sunx86_511],
   [AS_CASE([x"$ASFLAGS"],
     [*-m64*],
       dnl On some Solaris-like systems, our 'as' may be GNU as or Sun as.
       dnl For Sun as, running 'as -m64' on x86 gives us amd64 code.
       dnl For GNU as, running 'as -m64' is an error, and we can run 'as --64'
       dnl instead (or also the default 'as' gives us amd64 code on amd64).
       dnl
       dnl So if ASFLAGS has -m64, make sure 'as -m64' works. If it doesn't;
       dnl try 'as --64' instead.
       [rm -f conftest.$ac_objext

	AC_MSG_CHECKING([for $AS $ASFLAGS])
	AS_IF([AC_RUN_LOG([echo 'pushq %rbp' | $AS $ASFLAGS -o conftest.$ac_objext /dev/stdin])],
	 [AC_MSG_RESULT([yes])],
	 [AC_MSG_RESULT([no])

	  dnl Replace -m64 with --64 and try again
	  ASFLAGS=${ASFLAGS/-m64/--64}
	  AC_MSG_CHECKING([for $AS $ASFLAGS])
	  AS_IF([AC_RUN_LOG([echo 'pushq %rbp' | $AS $ASFLAGS -o conftest.$ac_objext /dev/stdin])],
	   [AC_MSG_RESULT([yes])],
	   [AC_MSG_RESULT([no])
	    AC_MSG_ERROR([Cannot run assembler, see config.log for details])])])])])
])
