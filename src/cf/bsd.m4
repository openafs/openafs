AC_DEFUN([OPENAFS_BSD_CHECKS],[
if test "x$with_bsd_kernel_headers" != "x"; then
	BSD_KERNEL_PATH="$with_bsd_kernel_headers"
else
	BSD_KERNEL_PATH="/usr/src/sys"
fi

if test "x$with_bsd_kernel_build" != "x"; then
	BSD_KERNEL_BUILD="$with_bsd_kernel_build"
else
	case $AFS_SYSNAME in
		*_fbsd_*)
			BSD_KERNEL_BUILD="${BSD_KERNEL_PATH}/${HOST_CPU}/compile/GENERIC"
			;;
		*_nbsd*)
			BSD_KERNEL_BUILD="${BSD_KERNEL_PATH}/arch/${HOST_CPU}/compile/GENERIC"
	esac
fi
])
