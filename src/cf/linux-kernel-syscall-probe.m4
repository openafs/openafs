AC_DEFUN([OPENAFS_LINUX_KERNEL_SYSCALL_PROBE_SETUP],[
dnl If we are guaranteed that keyrings will work - that is
dnl  a) The kernel has keyrings enabled
dnl  b) The code is new enough to give us a key_type_keyring
dnl then we just disable syscall probing unless we've been
dnl told otherwise

AS_IF([test "$enable_linux_syscall_probing" = "maybe"],
  [AS_IF([test "$ac_cv_linux_keyring_support" = "yes" -a "$ac_cv_linux_exports_key_type_keyring" = "yes"],
         [enable_linux_syscall_probing="no"],
         [enable_linux_syscall_probing="yes"])
])

dnl Syscall probing needs a few tests of its own, and just
dnl won't work if the kernel doesn't export init_mm
AS_IF([test "$enable_linux_syscall_probing" = "yes"], [
  LINUX_EXPORTS_INIT_MM
  AS_IF([test "$ac_cv_linux_exports_init_mm" = "no"], [
    AC_MSG_ERROR(
      [Can't do syscall probing without exported init_mm])
  ])
  LINUX_EXPORTS_SYS_CHDIR
  LINUX_EXPORTS_SYS_OPEN
  AC_DEFINE(ENABLE_LINUX_SYSCALL_PROBING, 1,
            [define to enable syscall table probes])
])
])

AC_DEFUN([OPENAFS_LINUX_KERNEL_SYSCALL_PROBE_CHECKS],[
dnl Syscall probing
if test "x$ac_cv_linux_config_modversions" = "xno" -o $AFS_SYSKVERS -ge 26; then
  AS_IF([test "$enable_linux_syscall_probing" = "yes"], [
    AC_MSG_WARN([Cannot determine sys_call_table status. assuming it isn't exported])
  ])
  ac_cv_linux_exports_sys_call_table=no
  if test -f "$LINUX_KERNEL_PATH/include/asm/ia32_unistd.h"; then
    ac_cv_linux_exports_ia32_sys_call_table=yes
  fi
else
  LINUX_EXPORTS_KALLSYMS_ADDRESS
  LINUX_EXPORTS_KALLSYMS_SYMBOL
  LINUX_EXPORTS_SYS_CALL_TABLE
  LINUX_EXPORTS_IA32_SYS_CALL_TABLE
  if test "x$ac_cv_linux_exports_sys_call_table" = "xno"; then
        linux_syscall_method=none
        if test "x$ac_cv_linux_exports_init_mm" = "xyes"; then
           linux_syscall_method=scan
           if test "x$ac_cv_linux_exports_kallsyms_address" = "xyes"; then
              linux_syscall_method=scan_with_kallsyms_address
           fi
        fi
        if test "x$ac_cv_linux_exports_kallsyms_symbol" = "xyes"; then
           linux_syscall_method=kallsyms_symbol
        fi
        if test "x$linux_syscall_method" = "xnone"; then
           AC_MSG_WARN([no available sys_call_table access method -- guessing scan])
           linux_syscall_method=scan
        fi
  fi
fi
])
