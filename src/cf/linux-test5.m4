
dnl AC_DEFUN([OPENAFS_GCC_SUPPORTS_NO_COMMON], [
dnl AC_MSG_CHECKING(if $CC supports -fno-common)
dnl save_CFLAGS="$CFLAGS"
dnl CFLAGS="-fno-common"
dnl AC_CACHE_VAL(openafs_gcc_supports_no_common,[
dnl AC_TRY_COMPILE(
dnl [],
dnl [int x;],
dnl openafs_gcc_supports_no_common=yes,
dnl openafs_gcc_supports_no_common=no)])
dnl AC_MSG_RESULT($openafs_gcc_supports_no_common)
dnl if test x$openafs_gcc_supports_no_common = xyes; then
dnl   LINUX_KCFLAGS="$LINUX_KCFLAGS -fno-common"
dnl fi
dnl CFLAGS="$save_CFLAGS"
dnl ])

AC_DEFUN([LINUX_KERNEL_HAS_NFSSRV], [
  AC_MSG_CHECKING(if kernel has nfs support)
  AC_CACHE_VAL([ac_cv_linux_kernel_has_nfssrv],[
    AC_TRY_KBUILD(
[#include <linux/sunrpc/svc.h>
#include <linux/sunrpc/svcauth.h>],
[#ifdef CONFIG_SUNRPC_SECURE
rpc_flavor_t x = 0;
struct auth_ops *ops = 0;
svc_auth_register(x, ops);
#else
lose
#endif],
      ac_cv_linux_kernel_has_nfssrv=yes,
      ac_cv_linux_kernel_has_nfssrv=no)])
  AC_MSG_RESULT($ac_cv_linux_kernel_has_nfssrv)])

AC_DEFUN([LINUX_KERNEL_GET_KCC], [
AC_MSG_NOTICE([kernel compilation options])
  if mkdir conftest.dir &&
    cat >conftest.dir/conftest.mk <<'_ACEOF' &&
include Makefile
cflags:; @echo CFLAGS=$[](CFLAGS)
cc:; @echo CC=$[](CC)
_ACEOF
    cat >conftest.dir/conftest.sh <<'_ACEOF' &&
KBUILD_SRC=$[]1
shift
make -C "$[]KBUILD_SRC" -f `pwd`/conftest.dir/conftest.mk KBUILD_SRC="$[]KBUILD_SRC" M=`pwd` V=1 "$[]@"
_ACEOF
    echo sh conftest.dir/conftest.sh $LINUX_KERNEL_PATH cc cflags >&AS_MESSAGE_LOG_FD
    sh conftest.dir/conftest.sh $LINUX_KERNEL_PATH cc cflags 2>conftest.err >conftest.out
    then
      LINUX_KCC="`sed -n 's/^CC=//p' conftest.out`"
      LINUX_KCFLAGS="`sed -n 's/^CFLAGS=//p' conftest.out`"
    else
      sed '/^ *+/d' conftest.err >&AS_MESSAGE_LOG_FD
      echo "$as_me: failed using conftestdir.dir/conftest.mk:" >&AS_MESSAGE_LOG_FD
      sed 's/^/| /' conftest.dir/conftest.mk >&AS_MESSAGE_LOG_FD
      echo "$as_me: and conftest.dir/conftest.sh was:" >&AS_MESSAGE_LOG_FD
      sed 's/^/| /' conftest.dir/conftest.sh >&AS_MESSAGE_LOG_FD
      AC_MSG_FAILURE([Fix problem or use --disable-kernel-module...])
  fi; rm -fr conftest.err conftest.out conftest.dir
  # for 2.6: empty LINUX_KCFLAGS or replace with fixed -Iarch/um/include
  if test -f $LINUX_KERNEL_PATH/scripts/Makefile.build; then
    LINUX_KCFLAGS=`echo "$LINUX_KCFLAGS" | sed "s/  */ /g
      s/"'$'"/ /
      : again
      h
      s/ .*//
      /-I[[^\/]]/{
	      s%-I%-I$LINUX_KERNEL_PATH/%
	      p
      }
      g
      s/^[[^ ]]* //
      t again
      d"`
  fi])

AC_DEFUN([OPENAFS_GCC_SUPPORTS_PIPE], [
AC_MSG_CHECKING(if $CC supports -pipe)
save_CFLAGS="$CFLAGS"
CFLAGS="-pipe"
AC_CACHE_VAL(openafs_gcc_supports_pipe,[
AC_TRY_COMPILE(
[],
[int x;],
openafs_gcc_supports_pipe=yes,
openafs_gcc_supports_pipe=no)])
AC_MSG_RESULT($openafs_gcc_supports_pipe)
if test x$openafs_gcc_supports_pipe = xyes; then
  LINUX_KCFLAGS="$LINUX_KCFLAGS -pipe"
fi
CFLAGS="$save_CFLAGS"
])
AC_SUBST(LINUX_KCC)
AC_SUBST(LINUX_KCFLAGS)
AC_SUBST(NFSSRV)
