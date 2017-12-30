AC_DEFUN([OPENAFS_LINUX_GUESS_VERSION],[
GUESS_LINUX_VERSION=
if test "x$enable_kernel_module" = "xyes"; then
    GUESS_LINUX_VERSION=${LINUX_VERSION}
else
    GUESS_LINUX_VERSION=`uname -r`
fi
case "$GUESS_LINUX_VERSION" in
    2.2.*) AFS_SYSKVERS=22 ;;
    2.4.*) AFS_SYSKVERS=24 ;;
    [2.6.* | [3-9]* | [1-2][0-9]*]) AFS_SYSKVERS=26 ;;
    *) AC_MSG_ERROR(Couldn't guess your Linux version [2]) ;;
esac
])
