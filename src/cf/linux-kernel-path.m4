AC_DEFUN([OPENAFS_LINUX_KERNEL_PATH],[
if test "x$with_linux_kernel_headers" != "x"; then
  LINUX_KERNEL_PATH="$with_linux_kernel_headers"
else
  for utsdir in "/lib/modules/`uname -r`/build" \
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
      enable_kernel_module="no"
    fi
  fi
fi
if test ! -f "$LINUX_KERNEL_BUILD/include/generated/autoconf.h" &&
   test ! -f "$LINUX_KERNEL_BUILD/include/linux/autoconf.h"; then
    enable_kernel_module="no"
fi
if test "x$enable_kernel_module" = "xno"; then
 if test "x$with_linux_kernel_headers" != "x"; then
  AC_MSG_ERROR(No usable linux headers found at $LINUX_KERNEL_PATH)
 else
  AC_MSG_WARN(No usable linux headers found at $LINUX_KERNEL_PATH so disabling kernel module)
 fi
fi
])
