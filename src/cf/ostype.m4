AC_DEFUN([OPENAFS_OSTYPE],[
AC_MSG_CHECKING(your OS)
system=$host
case $system in
        *-linux*)

		MKAFS_OSTYPE=LINUX
		if test "x$enable_redhat_buildsys" = "xyes"; then
		 AC_DEFINE(ENABLE_REDHAT_BUILDSYS, 1, [define if you have redhat buildsystem])
		fi
		if test "x$enable_kernel_module" = "xyes"; then
		 if test "x$with_linux_kernel_headers" != "x"; then
		   LINUX_KERNEL_PATH="$with_linux_kernel_headers"
		 else
		   for utsdir in "/lib/modules/`uname -r`/build" \
		                 "/lib/modules/`uname -r`/source" \
		                 "/usr/src/linux-2.4" \
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
		             exit 1
		           else
		             LINUX_VERSION="$linux_kvers"
                           fi
                         else
                           AC_MSG_ERROR(Linux headers lack version definition)
		           exit 1
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
		   exit 1
		  else
		   AC_MSG_WARN(No usable linux headers found at $LINUX_KERNEL_PATH so disabling kernel module)
		  fi
		 fi
                 dnl do we need to determine SUBARCH from autoconf.h
                 SUBARCH=default
		fi
		AC_MSG_RESULT(linux)
                if test "x$enable_kernel_module" = "xyes"; then
                 case "$LINUX_VERSION" in
                  2.2.*) AFS_SYSKVERS=22 ;;
                  2.4.*) AFS_SYSKVERS=24 ;;
                  [2.6.* | [3-9]* | [1-2][0-9]*]) AFS_SYSKVERS=26 ;;
                  *) AC_MSG_ERROR(Couldn't guess your Linux version [2]) ;;
                 esac
                fi
                ;;
        *-solaris*)
		MKAFS_OSTYPE=SOLARIS
                AC_MSG_RESULT(sun4)
		SOLARIS_PATH_CC
		SOLARIS_CC_TAKES_XVECTOR_NONE
		AC_SUBST(SOLARIS_CC_KOPTS)
		SOLARIS_UFSVFS_HAS_DQRWLOCK
		SOLARIS_FS_HAS_FS_ROLLED
		SOLARIS_SOLOOKUP_TAKES_SOCKPARAMS
		SOLARIS_HAVE_VN_RENAMEPATH
                ;;
        *-sunos*)
		MKAFS_OSTYPE=SUNOS
		enable_kernel_module=no
                AC_MSG_RESULT(sun4)
                ;;
        *-hpux*)
		MKAFS_OSTYPE=HPUX
                AC_MSG_RESULT(hp_ux)
		if test -f "/usr/old/usr/include/ndir.h"; then
		 AC_DEFINE(HAVE_USR_OLD_USR_INCLUDE_NDIR_H, 1, [define if you have old ndir.h])
		fi
                ;;
        *-irix*)
		if test -d /usr/include/sys/SN/SN1; then
		 IRIX_BUILD_IP35="IP35"
		fi
		MKAFS_OSTYPE=IRIX
                AC_MSG_RESULT(sgi)
                ;;
        *-aix*)
		MKAFS_OSTYPE=AIX
                AC_MSG_RESULT(rs_aix)
                ;;
        arm-*-darwin*)
		MKAFS_OSTYPE=DARWIN
                AC_MSG_RESULT(arm_darwin)
                ;;
        powerpc-*-darwin*)
		MKAFS_OSTYPE=DARWIN
                AC_MSG_RESULT(ppc_darwin)
                ;;
        i386-*-darwin*)
		MKAFS_OSTYPE=DARWIN
                AC_MSG_RESULT(x86_darwin)
                ;;
        x86_64-*-darwin*)
		MKAFS_OSTYPE=DARWIN
                AC_MSG_RESULT(x86_darwin)
                ;;
	i386-*-freebsd*)
		MKAFS_OSTYPE=FBSD
		AC_MSG_RESULT(i386_fbsd)
		;;
	x86_64-*-freebsd*)
		MKAFS_OSTYPE=FBSD
		AC_MSG_RESULT(amd64_fbsd)
		;;
	*-netbsd*)
		MKAFS_OSTYPE=NBSD
		AC_MSG_RESULT(nbsd)
		;;
	x86_64-*-openbsd*)
		MKAFS_OSTYPE=OBSD
		AC_MSG_RESULT(amd64_obsd)
		;;
	i386-*-openbsd*)
		MKAFS_OSTYPE=OBSD
		AC_MSG_RESULT(i386_obsd)
		;;
	*-dragonfly*)
		MKAFS_OSTYPE=DFBSD
		AC_MSG_RESULT(i386_dfbsd)
		;;
        *)
                AC_MSG_RESULT($system)
                ;;
esac
])
