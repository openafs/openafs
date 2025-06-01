# AC_TRY_KBUILD26([INCLUDES], [FUNCTION-BODY],
#                 [ACTION-IF-SUCCESS], [ACTION-IF-FAILURE])
#
AC_DEFUN([AC_TRY_KBUILD26],[  rm -fr conftest.dir
  if mkdir conftest.dir &&
    cat >conftest.dir/Makefile <<_ACEOF &&
${ac_linux_kbuild_cflags_var} += $CPPFLAGS

obj-m += conftest.o
_ACEOF
    cat >conftest.dir/conftest.c <<\_ACEOF &&
/* confdefs.h */
_ACEOF
    cat confdefs.h >>conftest.dir/conftest.c &&
    cat >>conftest.dir/conftest.c <<\_ACEOF &&
/* end confdefs.h */
#include <linux/module.h>
$1
void conftest(void);
void conftest(void)
{ 
$2
} 

MODULE_LICENSE("http://www.openafs.org/dl/license10.html");
MODULE_DESCRIPTION("openafs conftest");
_ACEOF
    echo make -C $LINUX_KERNEL_BUILD M=$SRCDIR_PARENT/conftest.dir $SPARSE_MAKEFLAGS modules KBUILD_VERBOSE=1 >&AS_MESSAGE_LOG_FD &&
    make -C $LINUX_KERNEL_BUILD M=$SRCDIR_PARENT/conftest.dir $SPARSE_MAKEFLAGS modules KBUILD_VERBOSE=1 >&AS_MESSAGE_LOG_FD 2>conftest.err &&
    ! grep -i "WARNING: .* undefined!$" conftest.err >/dev/null 2>&1
    then [$3]
    else
      sed '/^ *+/d' conftest.err >&AS_MESSAGE_LOG_FD
      echo "$as_me: failed using Makefile:" >&AS_MESSAGE_LOG_FD
      sed 's/^/| /' conftest.dir/Makefile >&AS_MESSAGE_LOG_FD
      echo "$as_me: and program was:" >&AS_MESSAGE_LOG_FD
      sed 's/^/| /' conftest.dir/conftest.c >&AS_MESSAGE_LOG_FD
      [$4]
  fi; rm -fr conftest.err conftest.dir])

  
# AC_TRY_KBUILD24([INCLUDES], [FUNCTION-BODY],
#                 [ACTION-IF-SUCCESS], [ACTION-IF-FAILURE])
#
AC_DEFUN([AC_TRY_KBUILD24], [
  ac_save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="-I$LINUX_KERNEL_PATH/include -D__KERNEL__ -Werror-implicit-function-declaration $CPPFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <linux/kernel.h>
$1]], [[$2]])],[$3],[$4])
  CPPFLAGS="$ac_save_CPPFLAGS"])


# AC_TRY_KBUILD([INCLUDES], [FUNCTION-BODY],
#               [ACTION-IF-SUCCESS], [ACTION-IF-FAILURE])
#
AC_DEFUN([AC_TRY_KBUILD], [
  if test $AFS_SYSKVERS -ge 26 ; then
    AC_TRY_KBUILD26([$1], [$2], [$3], [$4])
  else
    AC_TRY_KBUILD24([$1], [$2], [$3], [$4])
  fi])

AC_DEFUN([LINUX_KERNEL_COMPILE_WORKS], [
  AC_MSG_CHECKING([if linux kernel module build works])
  AC_TRY_KBUILD(
[#include <linux/sched.h>
#include <linux/fs.h>],
    [],:,AC_MSG_RESULT(no)
    AC_MSG_FAILURE([Fix problem or use --disable-kernel-module...]))
  AC_MSG_RESULT(yes)])

AC_DEFUN([AC_LINUX_KBUILD_CHECK_CFLAGS],[
    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS=-DCHECK_OPENAFS_CFLAGS
    AC_TRY_KBUILD(
[#include <linux/sched.h>
#include <linux/fs.h>
#ifndef CHECK_OPENAFS_CFLAGS
# error cflags not passed to the compiler
#endif
],
	[],
	$1, $2
    )
    CPPFLAGS="$save_CPPFLAGS"
])

AC_DEFUN([LINUX_KBUILD_FIND_CFLAGS_METHOD],
   [AC_MSG_CHECKING([how to set cflags for linux kbuild])
    ac_linux_kbuild_cflags_var=ccflags-y
    AC_LINUX_KBUILD_CHECK_CFLAGS(
	 [AC_MSG_RESULT([ccflags-y])],
	 [ac_linux_kbuild_cflags_var=EXTRA_CFLAGS
	  AC_LINUX_KBUILD_CHECK_CFLAGS(
	      [AC_MSG_RESULT([EXTRA_CFLAGS])],
	      [ac_linux_kbuild_cflags_var=CFLAGS
              AC_LINUX_KBUILD_CHECK_CFLAGS(
		  [AC_MSG_RESULT([CFLAGS])],
                  [ac_linux_kbuild_cflags_var=
		  AC_MSG_ERROR([cannot find way to set linux kbuild cflags])])
	      ])
	])
    LINUX_KBUILD_CFLAGS_VAR=$ac_linux_kbuild_cflags_var
    AC_SUBST([LINUX_KBUILD_CFLAGS_VAR])
])


AC_DEFUN([LINUX_KBUILD_SPARSE_CHECKS], [
  AC_ARG_WITH([sparse],
              AS_HELP_STRING([--with-sparse@<:@=PATH@:>@],
                             [Location of the 'sparse' tool]),
              [SPARSE="$withval"],
              [SPARSE=check])

  AS_CASE([$SPARSE],
          [check], [AC_PATH_PROG([SPARSE], [sparse], [])],
          [yes],   [AC_PATH_PROG([SPARSE], [sparse], [])
                    AS_IF([test "x$SPARSE" = "x"], [AC_MSG_ERROR([sparse not found])])],
          [no],    [SPARSE=])

  AS_IF([test x"$SPARSE" != x],
        [SPARSE_MAKEFLAGS="C=2 CHECK=$SPARSE"])

  AC_SUBST([SPARSE_MAKEFLAGS])

  AC_ARG_VAR([SPARSE], [Path to the 'sparse' tool])
])


dnl AC_CHECK_LINUX_BUILD([msg], [var], [includes], [code],
dnl                      [define], [description], [CFLAGS])
AC_DEFUN([AC_CHECK_LINUX_BUILD],
 [AS_VAR_PUSHDEF([ac_linux_build], [$2])dnl
  AC_CACHE_CHECK([$1], [ac_linux_build],
   [save_CPPFLAGS="$CPPFLAGS"
     CPPFLAGS="$CPPFLAGS -Wno-error $7"
     AC_TRY_KBUILD([$3], [$4],
		  AS_VAR_SET([ac_linux_build], [yes]),
		  AS_VAR_SET([ac_linux_build], [no]))
     CPPFLAGS="$save_CPPFLAGS"
   ])
  AS_IF([test AS_VAR_GET([ac_linux_build]) = yes],
        [AC_DEFINE([$5],1,[$6])])
 ])

dnl AC_CHECK_LINUX_HEADER(header)
AC_DEFUN([AC_CHECK_LINUX_HEADER],
 [AC_CHECK_LINUX_BUILD([for linux/$1], [ac_cv_linux_header_$1],
		       [#include <linux/$1>],
		       [return;],
		       AS_TR_CPP(HAVE_LINUX_$1),
		       [Define if your kernel has linux/$1])
 ])

dnl AC_CHECK_LINUX_FUNC([function], [includes], [code])
AC_DEFUN([AC_CHECK_LINUX_FUNC],
 [AS_VAR_PUSHDEF([ac_linux_func], [ac_cv_linux_func_$1])dnl
  AC_CACHE_CHECK([for $1], [ac_linux_func],
    [save_CPPFLAGS="$CPPFLAGS"
     CPPFLAGS="$CPPFLAGS -Wno-error -Werror-implicit-function-declaration"
     AC_TRY_KBUILD([$2], [$3],
		   AS_VAR_SET([ac_linux_func], [yes]),
		   AS_VAR_SET([ac_linux_func], [no]))
     CPPFLAGS="$save_CPPFLAGS"
    ])
  AS_IF([test AS_VAR_GET([ac_linux_func]) = yes],
	[AC_DEFINE(AS_TR_CPP(HAVE_LINUX_$1), 1,
		   [Define if your kernel has the $1 function])])
 ])

dnl AC_CHECK_LINUX_TYPE([type], [includes])
AC_DEFUN([AC_CHECK_LINUX_TYPE],
 [AC_CHECK_LINUX_BUILD([for $1],
                       [ac_cv_linux_type_$1_exists],
                       [#include <linux/$2>],
                       [static $1 _test; ],
                       AS_TR_CPP(HAVE_LINUX_$1),
                       [Define if kernel defines $1])
 ])

dnl AC_CHECK_LINUX_STRUCT([structure], [element], [includes])
AC_DEFUN([AC_CHECK_LINUX_STRUCT],
 [AC_CHECK_LINUX_TYPED_STRUCT([struct $1], [$2], [$3])
 ])

dnl AC_CHECK_LINUX_TYPED_STRUCT([structure], [element], [includes])
AC_DEFUN([AC_CHECK_LINUX_TYPED_STRUCT],
 [AC_CHECK_LINUX_BUILD([for $2 in $1],
		       [ac_cv_linux_$1_has_$2],
		       [#include <linux/$3>],
		       [static $1 _test; printk("%x\n", &_test.$2); ],
		       AS_TR_CPP($1_HAS_$2),
		       [Define if kernel $1 has the $2 element])
 ])

dnl AC_CHECK_LINUX_OPERATION([structure], [operation], [label], [includes], [return_type], [args])
AC_DEFUN([AC_CHECK_LINUX_OPERATION],
 [AS_VAR_PUSHDEF([ac_linux_operation], [ac_cv_linux_operation_$1_$2_$3])
  AC_CACHE_CHECK([operation $2 in $1], [ac_linux_operation],
    [save_CPPFLAGS="$CPPFLAGS"
     CPPFLAGS="$CPPFLAGS -Werror"
     AC_TRY_KBUILD(
      [$4
       static $5 op($6) { return ($5)0; };],
      [static struct $1 ops;  ops.$2 = op;],
		   AS_VAR_SET([ac_linux_operation], [yes]),
		   AS_VAR_SET([ac_linux_operation], [no]))
     CPPFLAGS="$save_CPPFLAGS"
    ])
  AS_IF([test AS_VAR_GET([ac_linux_operation]) = yes],
	[AC_DEFINE(AS_TR_CPP(HAVE_LINUX_$1_$2_$3), 1,
		   [Define if $1 has $2 operation of form $6])])
 ])
