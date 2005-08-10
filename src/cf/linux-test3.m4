AC_DEFUN([LINUX_KERNEL_LINUX_SYSCALL_H],[
  AC_MSG_CHECKING(for linux/syscall.h in kernel)
  if test -f "${LINUX_KERNEL_PATH}/include/linux/syscall.h"; then
    ac_linux_syscall=yes
    AC_MSG_RESULT($ac_linux_syscall)
  else
    ac_linux_syscall=no
    AC_MSG_RESULT($ac_linux_syscall)
  fi
])

AC_DEFUN([LINUX_NEED_RHCONFIG],[
RHCONFIG_SP=""
RHCONFIG_MP=""
if test "x$enable_redhat_buildsys" = "xyes"; then
  AC_MSG_WARN(Configured to build from a Red Hat SPEC file)
else
  AC_MSG_CHECKING(for redhat kernel configuration)
  if test -f "${LINUX_KERNEL_PATH}/include/linux/rhconfig.h"; then
    ac_linux_rhconfig=yes
    RHCONFIG_SP="-D__BOOT_KERNEL_UP=1 -D__BOOT_KERNEL_SMP=0"
    RHCONFIG_MP="-D__BOOT_KERNEL_UP=0 -D__BOOT_KERNEL_SMP=1"
    AC_MSG_RESULT($ac_linux_rhconfig)
    if test ! -f "/boot/kernel.h"; then
        AC_MSG_WARN([/boot/kernel.h does not exist. build may fail])
    fi
  else
    ac_linux_rhconfig=no
    AC_MSG_RESULT($ac_linux_rhconfig)
  fi
fi
AC_SUBST(RHCONFIG_SP)
AC_SUBST(RHCONFIG_MP)
])

AC_DEFUN([LINUX_WHICH_MODULES],[
if test "x$enable_redhat_buildsys" = "xyes"; then
  MPS=Default
else
  save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $RHCONFIG_SP $CPPFLAGS"
  AC_MSG_CHECKING(which kernel modules to build)
  if test "x$ac_linux_rhconfig" = "xyes"; then
      MPS="MP SP"
  else
  AC_CACHE_VAL(ac_cv_linux_config_smp, [
  AC_TRY_COMPILE(
[#include <linux/config.h>
],
[#ifndef CONFIG_SMP
lose;
#endif
],
  ac_cv_linux_config_smp=yes,
  ac_cv_linux_config_smp=no)])
  dnl AC_MSG_RESULT($ac_cv_linux_config_smp)
      if test "x$ac_cv_linux_config_smp" = "xyes"; then
          MPS=MP
      else
          MPS=SP
      fi
  fi
  CPPFLAGS=$save_CPPFLAGS
  AC_MSG_RESULT($MPS)
fi
AC_SUBST(MPS)
])

AC_DEFUN([LINUX_KERNEL_SELINUX],[
AC_MSG_CHECKING(for SELinux kernel)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_kernel_is_selinux,
[
AC_TRY_COMPILE(
  [#include <linux/autoconf.h>],
  [#ifndef CONFIG_SECURITY_SELINUX
   #error not SELINUX
   #endif],
  ac_cv_linux_kernel_is_selinux=yes,
  ac_cv_linux_kernel_is_selinux=no)])
AC_MSG_RESULT($ac_cv_linux_kernel_is_selinux)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_KERNEL_SOCK_CREATE],[
AC_MSG_CHECKING(for 5th argument in sock_create found in some SELinux kernels)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_kernel_sock_create_v,
[
AC_TRY_COMPILE(
  [#include <linux/net.h>],
  [
  sock_create(0,0,0,0,0)
  ],
  ac_cv_linux_kernel_sock_create_v=yes,
  ac_cv_linux_kernel_sock_create_v=no)])
AC_MSG_RESULT($ac_cv_linux_kernel_sock_create_v)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_KERNEL_PAGE_FOLLOW_LINK],[
AC_MSG_CHECKING(for page_follow_link_light vs page_follow_link)
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -I${LINUX_KERNEL_PATH}/include/asm/mach-default -Werror-implicit-function-declaration -D__KERNEL__ $CPPFLAGS"
AC_CACHE_VAL(ac_cv_linux_kernel_page_follow_link,
[
AC_TRY_COMPILE(
  [#include <linux/fs.h>],
  [
  page_follow_link(0,0)
  ],
  ac_cv_linux_kernel_page_follow_link=yes,
  ac_cv_linux_kernel_page_follow_link=no)])
AC_MSG_RESULT($ac_cv_linux_kernel_page_follow_link)
CPPFLAGS="$save_CPPFLAGS"])

AC_DEFUN([LINUX_KERNEL_LINUX_SEQ_FILE_H],[
  AC_MSG_CHECKING(for linux/seq_file.h in kernel)
  if test -f "${LINUX_KERNEL_PATH}/include/linux/seq_file.h"; then
    ac_linux_seq_file=yes
    AC_MSG_RESULT($ac_linux_seq_file)
  else
    ac_linux_seq_file=no
    AC_MSG_RESULT($ac_linux_seq_file)
  fi
])
