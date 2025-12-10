
# _OPENAFS_LINUX_KERNEL_PATHS()
# ----------------------------------------------------------------------------
# Find the path to Linux kernel headers and build directories and get the
# Linux kernel version of the build directory.
#
# Checks the following configure options:
#
#   --with-linux-kernel-headers=<path>
#   --with-linux-kernel-build=<path>
#
# Sets the following variables:
#
#   LINUX_KERNEL_PATH   Path to the Linux kernel headers
#   LINUX_KERNEL_BUILD  Path to the Linux module build directory
#   LINUX_VERSION       Linux kernel version in located build directory
#
# Fails with an error message if the LINUX_VERSION was not found and
# --with-linux-kernel-headers or --with-linux-kernel-build paths
# were specified.
#
AC_DEFUN([_OPENAFS_LINUX_KERNEL_PATHS], [
if test "x$with_linux_kernel_headers" != "x"; then
  LINUX_KERNEL_PATH="$with_linux_kernel_headers"
else
  for utsdir in "/usr/src/kernels/`uname -r`" \
                "/lib/modules/`uname -r`/build" \
                "/lib/modules/`uname -r`/source" \
                "/usr/src/linux"; do
    LINUX_KERNEL_PATH="$utsdir"
    for utsfile in "include/generated/utsrelease.h" \
                   "include/linux/utsrelease.h" \
                   "include/linux/version.h" \
                   "include/linux/version-up.h"; do
      if grep "UTS_RELEASE" "$utsdir/$utsfile" >/dev/null 2>&1; then
        break 2
      fi
    done
  done
fi
if test "x$with_linux_kernel_build" != "x"; then
  LINUX_KERNEL_BUILD="$with_linux_kernel_build"
else
  LINUX_KERNEL_BUILD=$LINUX_KERNEL_PATH
fi
if test -f "$LINUX_KERNEL_BUILD/include/generated/utsrelease.h"; then
  linux_kvers=`fgrep UTS_RELEASE $LINUX_KERNEL_BUILD/include/generated/utsrelease.h |awk 'BEGIN { FS="\"" } { print $[]2 }'|tail -n 1`
  LINUX_VERSION="$linux_kvers"
else
  if test -f "$LINUX_KERNEL_BUILD/include/linux/utsrelease.h"; then
    linux_kvers=`fgrep UTS_RELEASE $LINUX_KERNEL_BUILD/include/linux/utsrelease.h |awk 'BEGIN { FS="\"" } { print $[]2 }'|tail -n 1`
    LINUX_VERSION="$linux_kvers"
  else
    if test -f "$LINUX_KERNEL_BUILD/include/linux/version.h"; then
      linux_kvers=`fgrep UTS_RELEASE $LINUX_KERNEL_BUILD/include/linux/version.h |awk 'BEGIN { FS="\"" } { print $[]2 }'|tail -n 1`
      if test "x$linux_kvers" = "x"; then
        if test -f "$LINUX_KERNEL_BUILD/include/linux/version-up.h"; then
          linux_kvers=`fgrep UTS_RELEASE $LINUX_KERNEL_BUILD/include/linux/version-up.h |awk 'BEGIN { FS="\"" } { print $[]2 }'|tail -n 1`
          if test "x$linux_kvers" = "x"; then
            AC_MSG_ERROR(Linux headers lack version definition [2])
          else
            LINUX_VERSION="$linux_kvers"
          fi
        else
          AC_MSG_ERROR(Linux headers lack version definition)
        fi
      else
        LINUX_VERSION="$linux_kvers"
      fi
    else
      LINUX_VERSION=""
    fi
  fi
fi
if test ! -f "$LINUX_KERNEL_BUILD/include/generated/autoconf.h" &&
   test ! -f "$LINUX_KERNEL_BUILD/include/linux/autoconf.h"; then
    LINUX_VERSION=""
fi

AS_IF([test "x$LINUX_VERSION" = "x"],
  [AS_IF([test "x$with_linux_kernel_headers" != "x"],
    [AC_MSG_ERROR(
      [No usable linux headers found at $with_linux_kernel_headers])])
  AS_IF([test "x$with_linux_kernel_build" != "x"],
    [AC_MSG_ERROR(
      [No usable linux build found at $with_linux_kernel_build])])])
])

# OPENAFS_LINUX_KERNEL_PATH()
# -----------------------------------------------------------------------------
# Check for Linux kernel header and build paths unless --disable-kernel-module
# was specified.
#
# Fails if kernel paths are not found and --enable-kernel-module was specified.
#
# Disables kernel module compilation if the kernel paths are not found and
# --enable-kernel-module was not specified.
#
AC_DEFUN([OPENAFS_LINUX_KERNEL_PATH], [
  AS_CASE([$enable_kernel_module],
    [yes],
      [_OPENAFS_LINUX_KERNEL_PATHS
      AS_IF([test "x$LINUX_VERSION" = "x"],
        [kvers=`uname -r`
        AC_MSG_ERROR(m4_normalize([
          Unable to locate linux kernel headers.
          Please install the headers for linux kernel version ${kvers},
          or specify --with-linux-kernel-headers/--with-linux-kernel-build
          to build a module for a different kernel version,
          or specify --disable-kernel-module to disable kernel module
          compilation.]))])],
    [maybe],
      [_OPENAFS_LINUX_KERNEL_PATHS
      AS_IF([test "x$LINUX_VERSION" = "x"],
        [AC_MSG_WARN(
          [No usable linux headers found so disabling kernel module compilation.])
        enable_kernel_module="no"],
        [enable_kernel_module="yes"])])
])
