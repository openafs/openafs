# AC_TRY_KBUILD26([INCLUDES], [FUNCTION-BODY],
#                 [ACTION-IF-SUCCESS], [ACTION-IF-FAILURE])
#
AC_DEFUN([AC_TRY_KBUILD26],[  rm -fr conftest.dir
  if mkdir conftest.dir &&
    cat >conftest.dir/Makefile <<_ACEOF &&
CFLAGS += $CPPFLAGS

obj-m += conftest.o
_ACEOF
    cat >conftest.dir/conftest.c <<\_ACEOF &&
#include <linux/module.h>
$1

void conftest(void)
{ 
$2
} 

MODULE_LICENSE("http://www.openafs.org/dl/license10.html");
_ACEOF
    echo make -C $LINUX_KERNEL_PATH M=$SRCDIR_PARENT/conftest.dir modules KBUILD_VERBOSE=1 >&AS_MESSAGE_LOG_FD
    make -C $LINUX_KERNEL_PATH M=$SRCDIR_PARENT/conftest.dir modules KBUILD_VERBOSE=1 >&AS_MESSAGE_LOG_FD 2>conftest.err
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
  CPPFLAGS="-I$LINUX_KERNEL_PATH/include -D__KERNEL__ $CPPFLAGS"
  AC_TRY_COMPILE([$1], [$2], [$3], [$4])
  CPPFLAGS="$ac_save_CPPFLAGS"])


# AC_TRY_KBUILD([INCLUDES], [FUNCTION-BODY],
#               [ACTION-IF-SUCCESS], [ACTION-IF-FAILURE])
#
AC_DEFUN([AC_TRY_KBUILD], [
  if test -f $LINUX_KERNEL_PATH/scripts/Makefile.build; then
    AC_TRY_KBUILD26([$1], [$2], [$3], [$4])
  else
    AC_TRY_KBUILD24([$1], [$2], [$3], [$4])
  fi])

AC_DEFUN([LINUX_KERNEL_COMPILE_WORKS], [
  AC_MSG_CHECKING([for linux kernel module build works])
  AC_TRY_KBUILD([],[],:,AC_MSG_RESULT(no)
    AC_MSG_FAILURE([Fix problem or use --disable-kernel-module...]))
  AC_MSG_RESULT(yes)])
