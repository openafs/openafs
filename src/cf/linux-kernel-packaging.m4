AC_DEFUN([OPENAFS_LINUX_KERNEL_PACKAGING_CHECKS],[
dnl Packaging and SMP build
if test "x$with_linux_kernel_packaging" != "xyes" ; then
    LINUX_WHICH_MODULES
else
    AC_SUBST(MPS,'SP')
fi
])
