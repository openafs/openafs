AC_DEFUN(LINUX_NEED_RHCONFIG,[
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
    RHCONFIG_SP=""
    RHCONFIG_MP=""
    AC_MSG_RESULT($ac_linux_rhconfig)
fi
AC_SUBST(RHCONFIG_SP)
AC_SUBST(RHCONFIG_MP)
])

AC_DEFUN(LINUX_WHICH_MODULES,[
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-I${LINUX_KERNEL_PATH}/include -D__KERNEL__ $RHCONFIG_SP $CPPFLAGS"
AC_MSG_CHECKING(if kernel uses MODVERSIONS)
AC_CACHE_VAL(ac_cv_linux_config_modversions,[
AC_TRY_COMPILE(
[#include <linux/config.h>
],
[#ifndef CONFIG_MODVERSIONS
lose;
#endif
],
ac_cv_linux_config_modversions=yes,
ac_cv_linux_config_modversions=no)])
AC_MSG_RESULT($ac_cv_linux_config_modversions)
AC_MSG_CHECKING(which kernel modules to build)
if test "x$ac_linux_rhconfig" = "xyes" -o "x$ac_cv_linux_config_modversions" = "xno"; then
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
AC_MSG_RESULT($MPS)
AC_SUBST(MPS)
CPPFLAGS=$save_CPPFLAGS])

