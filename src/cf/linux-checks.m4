
AC_DEFUN([_OPENAFS_LINUX_CONFTEST_SETUP],[
# Add (sub-) architecture-specific paths needed by conftests
case $AFS_SYSNAME  in
    *_umlinux26)
        UMLINUX26_FLAGS="-I$LINUX_KERNEL_PATH/arch/um/include"
        UMLINUX26_FLAGS="$UMLINUX26_FLAGS -I$LINUX_KERNEL_PATH/arch/um/kernel/tt/include"
        UMLINUX26_FLAGS="$UMLINUX26_FLAGS -I$LINUX_KERNEL_PATH/arch/um/kernel/skas/include"
        CPPFLAGS="$CPPFLAGS $UMLINUX26_FLAGS"
esac
])

AC_DEFUN([_OPENAFS_LINUX_KBUILD_SETUP],[
if test "x$enable_debug_kernel" = "xno"; then
    LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -fomit-frame-pointer"
fi
AX_APPEND_COMPILE_FLAGS([-fno-strict-aliasing -fno-strength-reduce \
                         -fno-common -pipe],
                        [LINUX_GCC_KOPTS])
AC_SUBST(LINUX_GCC_KOPTS)

dnl Setup the kernel build environment
LINUX_KBUILD_USES_EXTRA_CFLAGS
LINUX_KERNEL_COMPILE_WORKS
])

AC_DEFUN([OPENAFS_LINUX_MISC_DEFINES],[
    if test "x$enable_linux_d_splice_alias_extra_iput" = xyes; then
        AC_DEFINE(D_SPLICE_ALIAS_LEAK_ON_ERROR, 1, [for internal use])
    fi
    dnl Linux-only, but just enable always.
    AC_DEFINE(AFS_CACHE_BYPASS, 1, [define to activate cache bypassing Unix client])
])

AC_DEFUN([OPENAFS_LINUX_CHECKS],[
case $AFS_SYSNAME in *_linux* | *_umlinux*)
    _OPENAFS_LINUX_CONFTEST_SETUP
    if test "x$enable_kernel_module" = "xyes"; then
        _OPENAFS_LINUX_KBUILD_SETUP
        OPENAFS_LINUX_KERNEL_SIG_CHECKS
        OPENAFS_LINUX_KERNEL_HEADER_CHECKS
        OPENAFS_LINUX_KERNEL_TYPE_CHECKS
        OPENAFS_LINUX_KERNEL_STRUCT_CHECKS
        OPENAFS_LINUX_KERNEL_FUNC_CHECKS
        OPENAFS_LINUX_KERNEL_ASSORTED_CHECKS
        OPENAFS_LINUX_KERNEL_SYSCALL_PROBE_SETUP
        OPENAFS_LINUX_KERNEL_PACKAGING_CHECKS
        OPENAFS_LINUX_KERNEL_SYSCALL_PROBE_CHECKS
        OPENAFS_LINUX_KERNEL_MORE_ASSORTED_CHECKS
    fi
    OPENAFS_LINUX_MISC_DEFINES
esac
])
