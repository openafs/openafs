# AC_TRY_KBUILD26([INCLUDES], [FUNCTION-BODY],
#                 [ACTION-IF-SUCCESS], [ACTION-IF-FAILURE])
#
AC_DEFUN([AC_TRY_KBUILD26], [
  rm -fr conftest.dir
  if mkdir conftest.dir; then
  cd conftest.dir
    cat >Makefile <<_ACEOF
CFLAGS += $CPPFLAGS

obj-m += conftest.o
_ACEOF
    cat >conftest.c <<\_ACEOF
$1

void conftest(void)
{ 
$2
} 
_ACEOF
    cd ..
  fi
  AS_IF(AC_RUN_LOG([make -C $LINUX_KERNEL_PATH M=`pwd`/conftest.dir modules > /dev/null]),
      [$3], [$4])
  rm -fr conftest.dir])

  
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
