dnl This file contains the common configuration code which would
dnl otherwise be duplicated between configure and configure-libafs.
dnl
dnl NB: Because this code is a macro, references to positional shell
dnl parameters must be done like $[]1 instead of $1

AC_DEFUN([OPENAFS_CONFIGURE_COMMON],[

AC_CANONICAL_HOST
SRCDIR_PARENT=`pwd`

#BOZO_SAVE_CORES pam
AC_ARG_WITH(afs-sysname,
[  --with-afs-sysname=sys    use sys for the afs sysname]
)
AC_ARG_ENABLE( afsdb,
[  --disable-afsdb 			disable AFSDB DNS RR support],, enable_afsdb="yes")
AC_ARG_ENABLE( pam,
[  --disable-pam 			disable PAM support],, enable_pam="yes")
AC_ARG_ENABLE( bos-restricted-mode,
[  --enable-bos-restricted-mode 	enable bosserver restricted mode which disables certain bosserver functionality],, enable_bos_restricted_mode="no")
AC_ARG_ENABLE( bos-new-config,
[  --enable-bos-new-config	 	enable bosserver pickup of BosConfig.new on restarts],, enable_bos_new_config="no")
AC_ARG_ENABLE( largefile-fileserver,
[  --disable-largefile-fileserver       disable large file support in fileserver],, enable_largefile_fileserver="yes")
AC_ARG_ENABLE( namei-fileserver,
[  --enable-namei-fileserver 		force compilation of namei fileserver in preference to inode fileserver],, enable_namei_fileserver="no")
AC_ARG_ENABLE( supergroups,
[  --enable-supergroups 		enable support for nested pts groups],, enable_supergroups="no")
AC_ARG_ENABLE( fast-restart,
[  --enable-fast-restart 		enable fast startup of file server without salvaging],, enable_fast_restart="no")
AC_ARG_ENABLE( bitmap-later,
[  --enable-bitmap-later 		enable fast startup of file server by not reading bitmap till needed],, enable_bitmap_later="no")
AC_ARG_ENABLE( demand-attach-fs,
[  --enable-demand-attach-fs 		enable Demand Attach Fileserver (please see documentation)],, enable_demand_attach_fs="no")
AC_ARG_ENABLE( unix-sockets,
[  --enable-unix-sockets 		enable use of unix domain sockets for fssync],, enable_unix_sockets="yes")
AC_ARG_ENABLE( full-vos-listvol-switch,
[  --disable-full-vos-listvol-switch    disable vos full listvol switch for formatted output],, enable_full_vos_listvol_switch="yes")
AC_ARG_WITH(dux-kernel-headers,
[  --with-dux-kernel-headers=path    	use the kernel headers found at path(optional, defaults to first match in /usr/sys)]
)
AC_ARG_WITH(linux-kernel-headers,
[  --with-linux-kernel-headers=path    	use the kernel headers found at path(optional, defaults to /usr/src/linux-2.4, then /usr/src/linux)]
)
AC_ARG_WITH(bsd-kernel-headers,
[  --with-bsd-kernel-headers=path    	use the kernel headers found at path(optional, defaults to /usr/src/sys)]
)
AC_ARG_WITH(bsd-kernel-build,
[  --with-bsd-kernel-build=path    	use the kernel build found at path(optional, defaults to KSRC/i386/compile/GENERIC)]
)
AC_ARG_ENABLE(kernel-module,
[  --disable-kernel-module             	disable compilation of the kernel module (defaults to enabled)],, enable_kernel_module="yes"
)
AC_ARG_ENABLE(redhat-buildsys,
[  --enable-redhat-buildsys		enable compilation of the redhat build system kernel (defaults to disabled)],, enable_redhat_buildsys="no"
)
AC_ARG_ENABLE(transarc-paths,
[  --enable-transarc-paths              	Use Transarc style paths like /usr/afs and /usr/vice],, enable_transarc_paths="no"
)
AC_ARG_ENABLE(tivoli-tsm,
[  --enable-tivoli-tsm              	Enable use of the Tivoli TSM API libraries for butc support],, enable_tivoli_tsm="no"
)
AC_ARG_ENABLE(debug-kernel,
[  --enable-debug-kernel		enable compilation of the kernel module with debugging information (defaults to disabled)],, enable_debug_kernel="no"
)
AC_ARG_ENABLE(optimize-kernel,
[  --disable-optimize-kernel		disable compilation of the kernel module with optimization (defaults based on platform)],, enable_optimize_kernel="yes"
)
AC_ARG_ENABLE(debug,
[  --enable-debug			enable compilation of the user space code with debugging information (defaults to disabled)],, enable_debug="no"
)
AC_ARG_ENABLE(strip-binaries,
[  --disable-strip-binaries             disable stripping of symbol information from binaries (defaults to enabled)],, enable_strip_binaries="maybe"
)
AC_ARG_ENABLE(optimize,
[  --disable-optimize			disable optimization for compilation of the user space code (defaults to enabled)],, enable_optimize="yes"
)
AC_ARG_ENABLE(debug-lwp,
[  --enable-debug-lwp			enable compilation of the LWP code with debugging information (defaults to disabled)],, enable_debug_lwp="no"
)
AC_ARG_ENABLE(optimize-lwp,
[  --disable-optimize-lwp		disable optimization for compilation of the LWP code (defaults to enabled)],, enable_optimize_lwp="yes"
)
AC_ARG_ENABLE(debug-pam,
[  --enable-debug-pam			enable compilation of the PAM code with debugging information (defaults to disabled)],, enable_debug_pam="no"
)
AC_ARG_ENABLE(optimize-pam,
[  --disable-optimize-pam		disable optimization for compilation of the PAM code (defaults to enabled)],, enable_optimize_pam="yes"
)


enable_login="no"

dnl weird ass systems
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl Various compiler setup.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL
COMPILER_HAS_FUNCTION_MACRO

dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB
AC_PROG_YACC
AM_PROG_LEX

OPENAFS_CHECK_BIGENDIAN

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
		   LINUX_KERNEL_PATH="/lib/modules/`uname -r`/source"
		   if test ! -f "$LINUX_KERNEL_PATH/include/linux/version.h"; then
		     LINUX_KERNEL_PATH="/lib/modules/`uname -r`/build"
		   fi
		   if test ! -f "$LINUX_KERNEL_PATH/include/linux/version.h"; then
		     LINUX_KERNEL_PATH="/usr/src/linux-2.4"
		   fi
		   if test ! -f "$LINUX_KERNEL_PATH/include/linux/version.h"; then
		     LINUX_KERNEL_PATH="/usr/src/linux"
		   fi
		 fi
               if test -f "$LINUX_KERNEL_PATH/include/linux/utsrelease.h"; then
		 linux_kvers=`fgrep UTS_RELEASE $LINUX_KERNEL_PATH/include/linux/utsrelease.h |awk 'BEGIN { FS="\"" } { print $[]2 }'|tail -n 1`
		 LINUX_VERSION="$linux_kvers"
               else
		 if test -f "$LINUX_KERNEL_PATH/include/linux/version.h"; then
		  linux_kvers=`fgrep UTS_RELEASE $LINUX_KERNEL_PATH/include/linux/version.h |awk 'BEGIN { FS="\"" } { print $[]2 }'|tail -n 1`
		  if test "x$linux_kvers" = "x"; then
		    if test -f "$LINUX_KERNEL_PATH/include/linux/version-up.h"; then
		      linux_kvers=`fgrep UTS_RELEASE $LINUX_KERNEL_PATH/include/linux/version-up.h |awk 'BEGIN { FS="\"" } { print $[]2 }'|tail -n 1`
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
		 if test ! -f "$LINUX_KERNEL_PATH/include/linux/autoconf.h"; then
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
                 AFS_SYSKVERS=`echo $LINUX_VERSION | awk -F\. '{print $[]1 $[]2}'`
                 if test "x${AFS_SYSKVERS}" = "x"; then
                  AC_MSG_ERROR(Couldn't guess your Linux version [2])
                 fi
                fi
                ;;
        *-solaris*)
		MKAFS_OSTYPE=SOLARIS
                AC_MSG_RESULT(sun4)
		SOLARIS_UFSVFS_HAS_DQRWLOCK
		SOLARIS_PROC_HAS_P_COREFILE
		SOLARIS_FS_HAS_FS_ROLLED
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
        *-osf*)
		MKAFS_OSTYPE=DUX
                AC_MSG_RESULT(alpha_dux)
                ;;
        powerpc-*-darwin*)
		MKAFS_OSTYPE=DARWIN
                AC_MSG_RESULT(ppc_darwin)
                ;;
        i386-*-darwin*)
		MKAFS_OSTYPE=DARWIN
                AC_MSG_RESULT(x86_darwin)
                ;;
	*-freebsd*)
		MKAFS_OSTYPE=FBSD
		AC_MSG_RESULT(i386_fbsd)
		;;
	*-netbsd*)
		MKAFS_OSTYPE=NBSD
		AC_MSG_RESULT(nbsd)
		;;
	*-openbsd*)
		MKAFS_OSTYPE=OBSD
		AC_MSG_RESULT(i386_obsd)
		;;
        *)
                AC_MSG_RESULT($system)
                ;;
esac

if test "x$with_afs_sysname" != "x"; then
        AFS_SYSNAME="$with_afs_sysname"
else
	AC_MSG_CHECKING(your AFS sysname)
	case $host in
		i?86-*-openbsd?.?)
			v=${host#*openbsd}
			vM=${v%.*}
			vm=${v#*.}
			AFS_SYSNAME="i386_obsd${vM}${vm}"
			;;
		sparc64-*-openbsd?.?)
			v=${host#*openbsd}
			vM=${v%.*}
			vm=${v#*.}
			AFS_SYSNAME="sparc64_obsd${vM}${vm}"
			;;
		i?86-*-freebsd?.*)
			v=${host#*freebsd}
			vM=${v%.*}
			vm=${v#*.}
			AFS_SYSNAME="i386_fbsd_${vM}${vm}"
			;;
		i?86-*-netbsd*1.5*)
			AFS_SYSNAME="i386_nbsd15"
			;;
		alpha-*-netbsd*1.5*)
			AFS_SYSNAME="alpha_nbsd15"
			;;
		i?86-*-netbsd*1.6[[M-Z]]*)
			AFS_SYSNAME="i386_nbsd20"
			;;
		powerpc-*-netbsd*1.6[[M-Z]]*)
			AFS_SYSNAME="ppc_nbsd20"
			;;
		i?86-*-netbsd*2.0*)
			AFS_SYSNAME="i386_nbsd20"
			;;
		amd64-*-netbsd*2.0*)
			AFS_SYSNAME="amd64_nbsd20"
			;;
		powerpc-*-netbsd*2.0*)
			AFS_SYSNAME="ppc_nbsd20"
			;;
		i?86-*-netbsd*1.6*)
			AFS_SYSNAME="i386_nbsd16"
			;;
		alpha-*-netbsd*1.6*)
			AFS_SYSNAME="alpha_nbsd16"
			;;
		powerpc-*-netbsd*1.6*)
			AFS_SYSNAME="ppc_nbsd16"
			;;
		i?86-*-netbsd*2.1*)
			AFS_SYSNAME="i386_nbsd21"
			;;
		i?86-*-netbsd*2.99*)
			AFS_SYSNAME="i386_nbsd30"
			;;
		i?86-*-netbsd*3.[[0-8]]*)
			AFS_SYSNAME="i386_nbsd30"
			;;
		i?86-*-netbsd*3.99*)
			AFS_SYSNAME="i386_nbsd30"
			;;
		i?86-*-netbsd*4.[[0-8]]*)
			AFS_SYSNAME="i386_nbsd40"
			;;
		i?86-*-netbsd*4.99*)
			AFS_SYSNAME="i386_nbsd40"
			;;
		hppa*-hp-hpux11.0*)
			AFS_SYSNAME="hp_ux110"
			;;
		hppa*-hp-hpux11.11)
			AFS_SYSNAME="hp_ux11i"
			;;
		ia64-hp-hpux11.22)
			AFS_SYSNAME="ia64_hpux1122"
			;;
		ia64-hp-hpux*)
			AFS_SYSNAME="ia64_hpux1123"
			;;
		hppa*-hp-hpux10*)
			AFS_SYSNAME="hp_ux102"
			;;
		powerpc-apple-darwin1.2*)
			AFS_SYSNAME="ppc_darwin_12"
			;;
		powerpc-apple-darwin1.3*)
			AFS_SYSNAME="ppc_darwin_13"
			;;
		powerpc-apple-darwin1.4*)
			AFS_SYSNAME="ppc_darwin_14"
			;;
		powerpc-apple-darwin5.1*)
			AFS_SYSNAME="ppc_darwin_14"
			;;
		powerpc-apple-darwin5.2*)
			AFS_SYSNAME="ppc_darwin_14"
			;;
		powerpc-apple-darwin5.3*)
			AFS_SYSNAME="ppc_darwin_14"
			;;
		powerpc-apple-darwin5.4*)
			AFS_SYSNAME="ppc_darwin_14"
			;;
		powerpc-apple-darwin5.5*)
			AFS_SYSNAME="ppc_darwin_14"
			;;
		powerpc-apple-darwin6.0*)
			AFS_SYSNAME="ppc_darwin_60"
			;;
		powerpc-apple-darwin6.1*)
			AFS_SYSNAME="ppc_darwin_60"
			;;
		powerpc-apple-darwin6.2*)
			AFS_SYSNAME="ppc_darwin_60"
			;;
		powerpc-apple-darwin6.3*)
			AFS_SYSNAME="ppc_darwin_60"
			;;
		powerpc-apple-darwin6.4*)
			AFS_SYSNAME="ppc_darwin_60"
			;;
		powerpc-apple-darwin6.5*)
			AFS_SYSNAME="ppc_darwin_60"
			;;
		powerpc-apple-darwin7.0*)
			AFS_SYSNAME="ppc_darwin_70"
			;;
		powerpc-apple-darwin7.1*)
			AFS_SYSNAME="ppc_darwin_70"
			;;
		powerpc-apple-darwin7.2*)
			AFS_SYSNAME="ppc_darwin_70"
			;;
		powerpc-apple-darwin7.3*)
			AFS_SYSNAME="ppc_darwin_70"
			;;
		powerpc-apple-darwin7.4*)
			AFS_SYSNAME="ppc_darwin_70"
			;;
		powerpc-apple-darwin7.5*)
			AFS_SYSNAME="ppc_darwin_70"
			;;
		powerpc-apple-darwin8.0*)
			AFS_SYSNAME="ppc_darwin_80"
			;;
		powerpc-apple-darwin8.*)
			AFS_SYSNAME="ppc_darwin_80"
			;;
		i386-apple-darwin8.*)
			AFS_SYSNAME="x86_darwin_80"
			;;
		powerpc-apple-darwin9.*)
			AFS_SYSNAME="ppc_darwin_90"
			;;
		i386-apple-darwin9.*)
			AFS_SYSNAME="x86_darwin_90"
			;;
		sparc-sun-solaris2.5*)
			AFS_SYSNAME="sun4x_55"
			enable_login="yes"
			;;
		sparc-sun-solaris2.6)
			AFS_SYSNAME="sun4x_56"
			;;
		sparc-sun-solaris2.7)
			AFS_SYSNAME="sun4x_57"
			;;
		sparc-sun-solaris2.8)
			AFS_SYSNAME="sun4x_58"
			;;
		sparc-sun-solaris2.9)
			AFS_SYSNAME="sun4x_59"
			;;
		sparc-sun-solaris2.10)
			AFS_SYSNAME="sun4x_510"
			;;
		sparc-sun-solaris2.11)
			AFS_SYSNAME="sun4x_511"
			;;
		sparc-sun-sunos4*)
			AFS_SYSNAME="sun4_413"
			enable_login="yes"
			;;
		i386-pc-solaris2.7)
			AFS_SYSNAME="sunx86_57"
			;;
		i386-pc-solaris2.8)
			AFS_SYSNAME="sunx86_58"
			;;
		i386-pc-solaris2.9)
			AFS_SYSNAME="sunx86_59"
			;;
		i386-pc-solaris2.10)
			AFS_SYSNAME="sunx86_510"
			;;
		i386-pc-solaris2.11)
			AFS_SYSNAME="sunx86_511"
			;;
		alpha*-dec-osf4.0*)
			AFS_SYSNAME="alpha_dux40"
			;;
		alpha*-dec-osf5.0*)
			AFS_SYSNAME="alpha_dux50"
			;;
		alpha*-dec-osf5.1*)
			AFS_SYSNAME="alpha_dux51"
			;;
		mips-sgi-irix6.5)
			AFS_SYSNAME="sgi_65"
			;;
		ia64-*-linux*)
			AFS_SYSNAME="ia64_linuxXX"
			;;
		powerpc-*-linux*)
			AFS_SYSNAME="`/bin/arch`_linuxXX"
			;;
		powerpc64-*-linux*)
			AFS_SYSNAME="ppc64_linuxXX"
			;;
		alpha*-linux*)
			AFS_SYSNAME="alpha_linux_XX"
			;;
		s390-*-linux*)
			AFS_SYSNAME="s390_linuxXX"
			;;
		s390x-*-linux*)
			AFS_SYSNAME="s390x_linuxXX"
			;;
		sparc-*-linux*)
			AFS_SYSNAME="`/bin/arch`_linuxXX"
			;;
		sparc64-*-linux*)
			AFS_SYSNAME="sparc64_linuxXX"
			;;
		i?86-*-linux*)
			AFS_SYSNAME="i386_linuxXX"
			;;
		parisc-*-linux-gnu|hppa-*-linux-gnu)
			AFS_SYSNAME="parisc_linuxXX"
			enable_pam="no"
			;;
		power*-ibm-aix4.2*)
			AFS_SYSNAME="rs_aix42"
			enable_pam="no"
			;;
		power*-ibm-aix4.3*)
			AFS_SYSNAME="rs_aix42"
			enable_pam="no"
			;;
		power*-ibm-aix5.1*)
			AFS_SYSNAME="rs_aix51"
			enable_pam="no"
			;;
		power*-ibm-aix5.2*)
			AFS_SYSNAME="rs_aix52"
			enable_pam="no"
			;;
		power*-ibm-aix5.3*)
			AFS_SYSNAME="rs_aix53"
			enable_pam="no"
			;;
		x86_64-*-linux-gnu)
			AFS_SYSNAME="amd64_linuxXX"
			enable_pam="no"
			;;
		*)
			AC_MSG_ERROR(An AFS sysname is required)
			exit 1
			;;
	esac
	case $AFS_SYSNAME in
		*_linux* | *_umlinux*)
			if test "x${AFS_SYSKVERS}" = "x"; then
			 AC_MSG_ERROR(Couldn't guess your Linux version. Please use the --with-afs-sysname option to configure an AFS sysname.)
			fi
			_AFS_SYSNAME=`echo $AFS_SYSNAME|sed s/XX\$/$AFS_SYSKVERS/`
			AFS_SYSNAME="$_AFS_SYSNAME"
			save_CPPFLAGS="$CPPFLAGS"
			CPPFLAGS="-I${LINUX_KERNEL_PATH}/include $CPPFLAGS"
			AC_TRY_COMPILE(
			 [#include <linux/autoconf.h>],
			 [#ifndef CONFIG_USERMODE
			  #error not UML
			  #endif],
			 ac_cv_linux_is_uml=yes,)
			if test "${ac_cv_linux_is_uml}" = yes; then
			 _AFS_SYSNAME=`echo $AFS_SYSNAME|sed s/linux/umlinux/`
			fi
			CPPFLAGS="$save_CPPFLAGS"
			AFS_SYSNAME="$_AFS_SYSNAME"
			;;
	esac
        AC_MSG_RESULT($AFS_SYSNAME)
fi

dnl Some hosts have a separate common param file they should include.  Figure
dnl out if we're on one of them now that we know the sysname.
case $AFS_SYSNAME in
    *_nbsd15)   AFS_PARAM_COMMON=param.nbsd15.h  ;;
    *_nbsd16)   AFS_PARAM_COMMON=param.nbsd16.h  ;;
    *_nbsd20)   AFS_PARAM_COMMON=param.nbsd20.h  ;;
    *_nbsd21)   AFS_PARAM_COMMON=param.nbsd21.h  ;;
    *_nbsd30)   AFS_PARAM_COMMON=param.nbsd30.h  ;;
    *_nbsd40)   AFS_PARAM_COMMON=param.nbsd40.h  ;;
    *_linux22)  AFS_PARAM_COMMON=param.linux22.h ;;
    *_linux24)  AFS_PARAM_COMMON=param.linux24.h ;;
    *_linux26)  AFS_PARAM_COMMON=param.linux26.h ;;
esac

case $AFS_SYSNAME in *_linux* | *_umlinux*)

		# Add (sub-) architecture-specific paths needed by conftests
		case $AFS_SYSNAME  in
			*_umlinux26)
				UMLINUX26_FLAGS="-I$LINUX_KERNEL_PATH/arch/um/include"
				UMLINUX26_FLAGS="$UMLINUX26_FLAGS -I$LINUX_KERNEL_PATH/arch/um/kernel/tt/include"
 				UMLINUX26_FLAGS="$UMLINUX26_FLAGS -I$LINUX_KERNEL_PATH/arch/um/kernel/skas/include"
				CPPFLAGS="$CPPFLAGS $UMLINUX26_FLAGS"
		esac

		if test "x$enable_kernel_module" = "xyes"; then
		 if test "x$enable_debug_kernel" = "xno"; then
			LINUX_GCC_KOPTS="$LINUX_GCC_KOPTS -fomit-frame-pointer"
		 fi
		 OPENAFS_GCC_SUPPORTS_MARCH
		 AC_SUBST(P5PLUS_KOPTS)
		 OPENAFS_GCC_NEEDS_NO_STRENGTH_REDUCE
		 OPENAFS_GCC_NEEDS_NO_STRICT_ALIASING
		 OPENAFS_GCC_SUPPORTS_NO_COMMON
		 OPENAFS_GCC_SUPPORTS_PIPE
		 AC_SUBST(LINUX_GCC_KOPTS)
	         ifdef([OPENAFS_CONFIGURE_LIBAFS],
	           [LINUX_BUILD_VNODE_FROM_INODE(src/config,src/afs)],
	           [LINUX_BUILD_VNODE_FROM_INODE(${srcdir}/src/config,src/afs/LINUX,${srcdir}/src/afs/LINUX)]
	         )

		 LINUX_KERNEL_COMPILE_WORKS
		 LINUX_HAVE_KMEM_CACHE_T
		 LINUX_KMEM_CACHE_CREATE_TAKES_DTOR
		 LINUX_CONFIG_H_EXISTS
		 LINUX_COMPLETION_H_EXISTS
		 LINUX_EXPORTFS_H_EXISTS
		 LINUX_DEFINES_FOR_EACH_PROCESS
		 LINUX_DEFINES_PREV_TASK
		 LINUX_FS_STRUCT_SUPER_HAS_ALLOC_INODE
	         LINUX_FS_STRUCT_ADDRESS_SPACE_HAS_PAGE_LOCK
	         LINUX_FS_STRUCT_ADDRESS_SPACE_HAS_GFP_MASK
		 LINUX_FS_STRUCT_INODE_HAS_I_ALLOC_SEM
		 LINUX_FS_STRUCT_INODE_HAS_I_BLKBITS
		 LINUX_FS_STRUCT_INODE_HAS_I_BLKSIZE
		 LINUX_FS_STRUCT_INODE_HAS_I_TRUNCATE_SEM
		 LINUX_FS_STRUCT_INODE_HAS_I_DIRTY_DATA_BUFFERS
		 LINUX_FS_STRUCT_INODE_HAS_I_DEVICES
		 LINUX_FS_STRUCT_INODE_HAS_I_MMAP_SHARED
		 LINUX_FS_STRUCT_INODE_HAS_I_MUTEX
		 LINUX_FS_STRUCT_INODE_HAS_I_SB_LIST
		 LINUX_FS_STRUCT_INODE_HAS_I_SECURITY
		 LINUX_FS_STRUCT_INODE_HAS_INOTIFY_LOCK
		 LINUX_FS_STRUCT_INODE_HAS_INOTIFY_SEM
	  	 LINUX_INODE_SETATTR_RETURN_TYPE
	  	 LINUX_WRITE_INODE_RETURN_TYPE
	  	 LINUX_IOP_I_CREATE_TAKES_NAMEIDATA
	  	 LINUX_IOP_I_LOOKUP_TAKES_NAMEIDATA
	  	 LINUX_IOP_I_PERMISSION_TAKES_NAMEIDATA
	  	 LINUX_IOP_I_PUT_LINK_TAKES_COOKIE
	  	 LINUX_DOP_D_REVALIDATE_TAKES_NAMEIDATA
	  	 LINUX_FOP_F_FLUSH_TAKES_FL_OWNER_T
	  	 LINUX_AOP_WRITEBACK_CONTROL
		 LINUX_FS_STRUCT_FOP_HAS_FLOCK
		 LINUX_FS_STRUCT_FOP_HAS_SENDFILE
		 LINUX_FS_STRUCT_FOP_HAS_SPLICE
		 LINUX_KERNEL_LINUX_SYSCALL_H
		 LINUX_KERNEL_LINUX_SEQ_FILE_H
		 LINUX_KERNEL_POSIX_LOCK_FILE_WAIT_ARG
		 LINUX_KERNEL_SELINUX
		 LINUX_KERNEL_SOCK_CREATE
		 LINUX_KERNEL_PAGE_FOLLOW_LINK
		 LINUX_NEED_RHCONFIG
		 LINUX_RECALC_SIGPENDING_ARG_TYPE
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_PARENT
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_REAL_PARENT
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIG
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGHAND
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGMASK_LOCK
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_RLIM
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_SIGNAL_RLIM
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_EXIT_STATE
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_TGID
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_TODO
		 LINUX_SCHED_STRUCT_TASK_STRUCT_HAS_THREAD_INFO
		 LINUX_EXPORTS_TASKLIST_LOCK
		 LINUX_GET_SB_HAS_STRUCT_VFSMOUNT
		 LINUX_STATFS_TAKES_DENTRY
		 LINUX_FREEZER_H_EXISTS
		 LINUX_HAVE_SVC_ADDR_IN
		 if test "x$ac_cv_linux_freezer_h_exists" = "xyes" ; then
		  AC_DEFINE(FREEZER_H_EXISTS, 1, [define if you have linux/freezer.h])
		 fi
		 LINUX_REFRIGERATOR
		 LINUX_LINUX_KEYRING_SUPPORT
		 LINUX_KEY_ALLOC_NEEDS_STRUCT_TASK
		 LINUX_DO_SYNC_READ
		 LINUX_GENERIC_FILE_AIO_READ
		 LINUX_INIT_WORK_HAS_DATA
		 LINUX_REGISTER_SYSCTL_TABLE_NOFLAG
                 LINUX_EXPORTS_SYS_CHDIR
                 LINUX_EXPORTS_SYS_CLOSE
                 LINUX_EXPORTS_SYS_OPEN
                 LINUX_EXPORTS_SYS_WAIT4
		 LINUX_WHICH_MODULES
                 if test "x$ac_cv_linux_config_modversions" = "xno" -o $AFS_SYSKVERS -ge 26; then
                   AC_MSG_WARN([Cannot determine sys_call_table status. assuming it isn't exported])
                   ac_cv_linux_exports_sys_call_table=no
		   if test -f "$LINUX_KERNEL_PATH/include/asm/ia32_unistd.h"; then
		     ac_cv_linux_exports_ia32_sys_call_table=yes
		   fi
                 else
                   LINUX_EXPORTS_INIT_MM
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
		 if test -f "$LINUX_KERNEL_PATH/include/linux/in_systm.h"; then
		  AC_DEFINE(HAVE_IN_SYSTM_H, 1, [define if you have in_systm.h header file])
	         fi
		 if test -f "$LINUX_KERNEL_PATH/include/linux/mm_inline.h"; then
		  AC_DEFINE(HAVE_MM_INLINE_H, 1, [define if you have mm_inline.h header file])
	         fi
		 if test -f "$LINUX_KERNEL_PATH/include/linux/in_systm.h"; then
		  AC_DEFINE(HAVE_IN_SYSTM_H, 1, [define if you have in_systm.h header file])
	         fi
		 if test "x$ac_cv_linux_exports_sys_chdir" = "xyes" ; then
		  AC_DEFINE(EXPORTED_SYS_CHDIR, 1, [define if your linux kernel exports sys_chdir])
		 fi
		 if test "x$ac_cv_linux_exports_sys_open" = "xyes" ; then
		  AC_DEFINE(EXPORTED_SYS_OPEN, 1, [define if your linux kernel exports sys_open])
		 fi
		 if test "x$ac_cv_linux_exports_sys_close" = "xyes" ; then
		  AC_DEFINE(EXPORTED_SYS_CLOSE, 1, [define if your linux kernel exports sys_close])
		 fi
		 if test "x$ac_cv_linux_exports_sys_wait4" = "xyes" ; then
		  AC_DEFINE(EXPORTED_SYS_WAIT4, 1, [define if your linux kernel exports sys_wait4])
		 fi
                 if test "x$ac_cv_linux_exports_sys_call_table" = "xyes"; then
                  AC_DEFINE(EXPORTED_SYS_CALL_TABLE)
                 fi
                 if test "x$ac_cv_linux_exports_ia32_sys_call_table" = "xyes"; then
                  AC_DEFINE(EXPORTED_IA32_SYS_CALL_TABLE)
                 fi
                 if test "x$ac_cv_linux_exports_kallsyms_symbol" = "xyes"; then
                  AC_DEFINE(EXPORTED_KALLSYMS_SYMBOL)
                 fi
                 if test "x$ac_cv_linux_exports_kallsyms_address" = "xyes"; then
                  AC_DEFINE(EXPORTED_KALLSYMS_ADDRESS)
                 fi
		 if test "x$ac_cv_linux_completion_h_exists" = "xyes" ; then
		  AC_DEFINE(COMPLETION_H_EXISTS, 1, [define if completion_h exists])
		 fi
		 if test "x$ac_cv_linux_config_h_exists" = "xyes" ; then
		  AC_DEFINE(CONFIG_H_EXISTS, 1, [define if config.h exists])
		 fi
		 if test "x$ac_cv_linux_exportfs_h_exists" = "xyes"; then
		  AC_DEFINE(EXPORTFS_H_EXISTS, 1, [define if linux/exportfs.h exists])
		 fi
		 if test "x$ac_cv_linux_defines_for_each_process" = "xyes" ; then
		  AC_DEFINE(DEFINED_FOR_EACH_PROCESS, 1, [define if for_each_process defined])
		 fi
		 if test "x$ac_cv_linux_defines_prev_task" = "xyes" ; then
		  AC_DEFINE(DEFINED_PREV_TASK, 1, [define if prev_task defined])
		 fi
		 if test "x$ac_cv_linux_func_inode_setattr_returns_int" = "xyes" ; then
		  AC_DEFINE(INODE_SETATTR_NOT_VOID, 1, [define if your setattr return return non-void])
		 fi
		 if test "x$ac_cv_linux_func_write_inode_returns_int" = "xyes" ; then
		  AC_DEFINE(WRITE_INODE_NOT_VOID, 1, [define if your sops.write_inode returns non-void])
		 fi
		 if test "x$ac_cv_linux_fs_struct_super_has_alloc_inode" = "xyes" ; then
		  AC_DEFINE(STRUCT_SUPER_HAS_ALLOC_INODE, 1, [define if your struct super_operations has alloc_inode])
		 fi
		 if test "x$ac_cv_linux_fs_struct_address_space_has_page_lock" = "xyes"; then 
		  AC_DEFINE(STRUCT_ADDRESS_SPACE_HAS_PAGE_LOCK, 1, [define if your struct address_space has page_lock])
		 fi
		 if test "x$ac_cv_linux_fs_struct_address_space_has_gfp_mask" = "xyes"; then 
		  AC_DEFINE(STRUCT_ADDRESS_SPACE_HAS_GFP_MASK, 1, [define if your struct address_space has gfp_mask])
		 fi
		 if test "x$ac_cv_linux_fs_struct_inode_has_i_truncate_sem" = "xyes"; then 
		  AC_DEFINE(STRUCT_INODE_HAS_I_TRUNCATE_SEM, 1, [define if your struct inode has truncate_sem])
		 fi
		 if test "x$ac_cv_linux_fs_struct_inode_has_i_alloc_sem" = "xyes"; then 
		  AC_DEFINE(STRUCT_INODE_HAS_I_ALLOC_SEM, 1, [define if your struct inode has alloc_sem])
		 fi
		 if test "x$ac_cv_linux_fs_struct_inode_has_i_blksize" = "xyes"; then 
		  AC_DEFINE(STRUCT_INODE_HAS_I_BLKSIZE, 1, [define if your struct inode has i_blksize])
		 fi
		 if test "x$ac_cv_linux_fs_struct_inode_has_i_devices" = "xyes"; then 
		  AC_DEFINE(STRUCT_INODE_HAS_I_DEVICES, 1, [define if you struct inode has i_devices])
		 fi
		 if test "x$ac_cv_linux_fs_struct_inode_has_i_security" = "xyes"; then 
		  AC_DEFINE(STRUCT_INODE_HAS_I_SECURITY, 1, [define if you struct inode has i_security])
		 fi
		 if test "x$ac_cv_linux_fs_struct_inode_has_i_mutex" = "xyes"; then 
		  AC_DEFINE(STRUCT_INODE_HAS_I_MUTEX, 1, [define if you struct inode has i_mutex])
		 fi
		 if test "x$ac_cv_linux_fs_struct_inode_has_i_sb_list" = "xyes"; then 
		  AC_DEFINE(STRUCT_INODE_HAS_I_SB_LIST, 1, [define if you struct inode has i_sb_list])
		 fi
		 if test "x$ac_cv_linux_fs_struct_inode_has_i_dirty_data_buffers" = "xyes"; then 
		  AC_DEFINE(STRUCT_INODE_HAS_I_DIRTY_DATA_BUFFERS, 1, [define if your struct inode has data_buffers])
		 fi
		 if test "x$ac_cv_linux_fs_struct_inode_has_inotify_lock" = "xyes"; then 
		  AC_DEFINE(STRUCT_INODE_HAS_INOTIFY_LOCK, 1, [define if your struct inode has inotify_lock])
		 fi
		 if test "x$ac_cv_linux_fs_struct_inode_has_inotify_sem" = "xyes"; then 
		  AC_DEFINE(STRUCT_INODE_HAS_INOTIFY_SEM, 1, [define if your struct inode has inotify_sem])
		 fi
		 if test "x$ac_cv_linux_func_recalc_sigpending_takes_void" = "xyes"; then 
		  AC_DEFINE(RECALC_SIGPENDING_TAKES_VOID, 1, [define if your recalc_sigpending takes void])
		 fi
		 if test "x$ac_cv_linux_kernel_posix_lock_file_wait_arg" = "xyes" ; then
		  AC_DEFINE(POSIX_LOCK_FILE_WAIT_ARG, 1, [define if your linux kernel uses 3 arguments for posix_lock_file])
		 fi
		 if test "x$ac_cv_linux_kernel_is_selinux" = "xyes" ; then
		  AC_DEFINE(LINUX_KERNEL_IS_SELINUX, 1, [define if your linux kernel uses SELinux features])
		 fi
		 if test "x$ac_cv_linux_kernel_sock_create_v" = "xyes" ; then
		  AC_DEFINE(LINUX_KERNEL_SOCK_CREATE_V, 1, [define if your linux kernel uses 5 arguments for sock_create])
		 fi
		 if test "x$ac_cv_linux_kernel_page_follow_link" = "xyes" ; then
		  AC_DEFINE(HAVE_KERNEL_PAGE_FOLLOW_LINK, 1, [define if your linux kernel provides page_follow_link])
		 fi
		 if test "x$ac_linux_syscall" = "xyes" ; then
		  AC_DEFINE(HAVE_KERNEL_LINUX_SYSCALL_H, 1, [define if your linux kernel has linux/syscall.h])
		 fi
		 if test "x$ac_linux_seq_file" = "xyes" ; then
		  AC_DEFINE(HAVE_KERNEL_LINUX_SEQ_FILE_H, 1, [define if your linux kernel has linux/seq_file.h])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_parent" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_PARENT, 1, [define if your struct task_struct has parent])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_real_parent" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_REAL_PARENT, 1, [define if your struct task_struct has real_parent])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_sigmask_lock" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_SIGMASK_LOCK, 1, [define if your struct task_struct has sigmask_lock])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_sighand" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_SIGHAND, 1, [define if your struct task_struct has sighand])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_sig" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_SIG, 1, [define if your struct task_struct has sig])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_rlim" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_RLIM, 1, [define if your struct task_struct has rlim])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_signal_rlim" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_SIGNAL_RLIM, 1, [define if your struct task_struct has signal->rlim])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_exit_state" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_EXIT_STATE, 1, [define if your struct task_struct has exit_state])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_tgid" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_TGID, 1, [define if your struct task_struct has tgid])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_todo" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_TODO, 1, [define if your struct task_struct has todo])
		 fi
		 if test "x$ac_cv_linux_sched_struct_task_struct_has_thread_info" = "xyes"; then 
		  AC_DEFINE(STRUCT_TASK_STRUCT_HAS_THREAD_INFO, 1, [define if your struct task_struct has thread_info])
		 fi
		 if test "x$ac_cv_linux_get_sb_has_struct_vfsmount" = "xyes"; then
		  AC_DEFINE(GET_SB_HAS_STRUCT_VFSMOUNT, 1, [define if your get_sb_nodev needs a struct vfsmount argument])
		 fi
		 if test "x$ac_cv_linux_statfs_takes_dentry" = "xyes"; then
		  AC_DEFINE(STATFS_TAKES_DENTRY, 1, [define if your statfs takes a dentry argument])
		 fi
		 if test "x$ac_cv_linux_func_a_writepage_takes_writeback_control" = "xyes" ; then
		  AC_DEFINE(AOP_WRITEPAGE_TAKES_WRITEBACK_CONTROL, 1, [define if your aops.writepage takes a struct writeback_control argument])
		 fi
		 if test "x$ac_cv_linux_func_refrigerator_takes_pf_freeze" = "xyes" ; then
		  AC_DEFINE(LINUX_REFRIGERATOR_TAKES_PF_FREEZE, 1, [define if your refrigerator takes PF_FREEZE])
		 fi
		 if test "x$ac_cv_linux_func_i_create_takes_nameidata" = "xyes" ; then
		  AC_DEFINE(IOP_CREATE_TAKES_NAMEIDATA, 1, [define if your iops.create takes a nameidata argument])
		 fi
		 if test "x$ac_cv_linux_func_f_flush_takes_fl_owner_t" = "xyes" ; then
		  AC_DEFINE(FOP_FLUSH_TAKES_FL_OWNER_T, 1, [define if your fops.flush takes an fl_owner_t argument])
		 fi
		 if test "x$ac_cv_linux_func_i_lookup_takes_nameidata" = "xyes" ; then
		  AC_DEFINE(IOP_LOOKUP_TAKES_NAMEIDATA, 1, [define if your iops.lookup takes a nameidata argument])
		 fi
		 if test "x$ac_cv_linux_func_i_permission_takes_nameidata" = "xyes" ; then
		  AC_DEFINE(IOP_PERMISSION_TAKES_NAMEIDATA, 1, [define if your iops.permission takes a nameidata argument])
		 fi
		 if test "x$ac_cv_linux_func_d_revalidate_takes_nameidata" = "xyes" ; then
		  AC_DEFINE(DOP_REVALIDATE_TAKES_NAMEIDATA, 1, [define if your dops.d_revalidate takes a nameidata argument])
		 fi
		 if test "x$ac_cv_linux_init_work_has_data" = "xyes" ; then
		  AC_DEFINE(INIT_WORK_HAS_DATA, 1, [define if INIT_WORK takes a data (3rd) argument])
		 fi
		 if test "x$ac_cv_linux_fs_struct_fop_has_flock" = "xyes" ; then
		  AC_DEFINE(STRUCT_FILE_OPERATIONS_HAS_FLOCK, 1, [define if your struct file_operations has flock])
		 fi
		 if test "x$ac_cv_linux_fs_struct_fop_has_sendfile" = "xyes" ; then
		  AC_DEFINE(STRUCT_FILE_OPERATIONS_HAS_SENDFILE, 1, [define if your struct file_operations has sendfile])
		 fi
		 if test "x$ac_cv_linux_fs_struct_fop_has_splice" = "xyes" ; then
		  AC_DEFINE(STRUCT_FILE_OPERATIONS_HAS_SPLICE, 1, [define if your struct file_operations has splice_write and splice_read])
		 fi
		 if test "x$ac_cv_linux_register_sysctl_table_noflag" = "xyes" ; then
		  AC_DEFINE(REGISTER_SYSCTL_TABLE_NOFLAG, 1, [define if register_sysctl_table has no insert_at head flag])
		 fi
		 if test "x$ac_cv_linux_exports_tasklist_lock" = "xyes" ; then
		  AC_DEFINE(EXPORTED_TASKLIST_LOCK, 1, [define if tasklist_lock exported])
		 fi
		 if test "x$ac_cv_linux_have_kmem_cache_t" = "xyes" ; then
		  AC_DEFINE(HAVE_KMEM_CACHE_T, 1, [define if kmem_cache_t exists])
		 fi
		 if test "x$ac_cv_linux_have_kmem_cache_t" = "xyes" ; then
		  AC_DEFINE(KMEM_CACHE_TAKES_DTOR, 1, [define if kmem_cache_create takes a destructor argument])
		 fi
		 if test "x$ac_cv_linux_kernel_page_follow_link" = "xyes" -o "x$ac_cv_linux_func_i_put_link_takes_cookie" = "xyes"; then
		  AC_DEFINE(USABLE_KERNEL_PAGE_SYMLINK_CACHE, 1, [define if your kernel has a usable symlink cache API])
		 else
		  AC_MSG_WARN([your kernel does not have a usable symlink cache API])
		 fi
		 if test "x$ac_cv_linux_have_svc_addr_in" = "xyes"; then
		  AC_DEFINE(HAVE_SVC_ADDR_IN, 1, [define if svc_add_in exists])
                 fi
                :
		fi
esac

case $AFS_SYSNAME in
	*_darwin*)
		DARWIN_PLIST=src/libafs/afs.${AFS_SYSNAME}.plist
		DARWIN_INFOFILE=afs.${AFS_SYSNAME}.plist
                dnl the test below fails on darwin, even if the CPPFLAGS below
                dnl are added. the headers from Kernel.framework must be used
                dnl when KERNEL is defined.

                dnl really, such a thing isn't guaranteed to work on any 
                dnl platform until the kernel cflags from MakefileProto are
                dnl known to configure
	        AC_DEFINE(HAVE_STRUCT_BUF, 1, [define if you have a struct buf])
		;;
        *)
AC_MSG_CHECKING(for definition of struct buf)
dnl save_CPPFLAGS="$CPPFLAGS"
dnl CPPFLAGS="$CPPFLAGS -DKERNEL -D_KERNEL -D__KERNEL -D__KERNEL__"
AC_CACHE_VAL(ac_cv_have_struct_buf, [
	ac_cv_have_struct_buf=no
	AC_TRY_COMPILE(
		[#include <sys/buf.h>],
		[struct buf x;
		printf("%d\n", sizeof(x));],
		ac_cv_have_struct_buf=yes,)
	]
)
dnl CPPFLAGS="$save_CPPFLAGS"
AC_MSG_RESULT($ac_cv_have_struct_buf)
if test "$ac_cv_have_struct_buf" = yes; then
	AC_DEFINE(HAVE_STRUCT_BUF, 1, [define if you have a struct buf])
fi
;;
esac


AC_CACHE_VAL(ac_cv_sockaddr_len,
[
AC_MSG_CHECKING([if struct sockaddr has sa_len field])
AC_TRY_COMPILE( [#include <sys/types.h>
#include <sys/socket.h>],
[struct sockaddr *a;
a->sa_len=0;], ac_cv_sockaddr_len=yes, ac_cv_sockaddr_len=no)
AC_MSG_RESULT($ac_cv_sockaddr_len)])
if test "$ac_cv_sockaddr_len" = "yes"; then
   AC_DEFINE(STRUCT_SOCKADDR_HAS_SA_LEN, 1, [define if you struct sockaddr sa_len])
fi
if test "x${MKAFS_OSTYPE}" = "xIRIX"; then
        echo Skipping library tests because they confuse Irix.
else
  AC_CHECK_FUNCS(socket)

  if test "$ac_cv_func_socket" = no; then
    for lib in socket inet; do
        if test "$HAVE_SOCKET" != 1; then
                AC_CHECK_LIB(${lib}, socket,LIBS="$LIBS -l$lib";HAVE_SOCKET=1;AC_DEFINE(HAVE_SOCKET, 1, [define if you have socket]))
        fi
    done
  fi
  
  AC_CHECK_FUNCS(connect)       

  if test "$ac_cv_func_connect" = no; then
    for lib in nsl; do
        if test "$HAVE_CONNECT" != 1; then
                AC_CHECK_LIB(${lib}, connect,LIBS="$LIBS -l$lib";HAVE_CONNECT=1;AC_DEFINE(HAVE_CONNECT, 1, [define if you have connect]))
        fi
    done
  fi

  AC_CHECK_FUNCS(gethostbyname)
  if test "$ac_cv_func_gethostbyname" = no; then
        for lib in dns nsl resolv; do
          if test "$HAVE_GETHOSTBYNAME" != 1; then
            AC_CHECK_LIB(${lib}, gethostbyname, LIBS="$LIBS -l$lib";HAVE_GETHOSTBYNAME=1;AC_DEFINE(HAVE_GETHOSTBYNAME, 1, [define if you have gethostbyname]))
          fi
        done    
  fi    

  dnl darwin wants it, aix hates it
  AC_MSG_CHECKING(for the useability of arpa/nameser_compat.h)
  AC_TRY_COMPILE([
  #include <stdlib.h>
  #include <stdio.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <arpa/nameser.h>
  #include <arpa/nameser_compat.h>
  #include <resolv.h>
  ], [static int i; i = 0;],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(HAVE_ARPA_NAMESER_COMPAT_H)],
  [AC_MSG_RESULT(no)
   ])

  openafs_save_libs="$LIBS"
  AC_MSG_CHECKING([for res_search])
  AC_FUNC_RES_SEARCH

  if test "$ac_cv_func_res_search" = no; then
      for lib in dns nsl resolv; do
        if test "$ac_cv_func_res_search" != yes; then
	  LIBS="-l$lib $LIBS"
          AC_FUNC_RES_SEARCH
          LIBS="$openafs_save_libs"
        fi
      done    
      if test "$ac_cv_func_res_search" = yes; then
        LIB_res_search="-l$lib"       
	AC_DEFINE(HAVE_RES_SEARCH, 1, [])
        AC_MSG_RESULT([yes, in lib$lib])
      else
        AC_MSG_RESULT(no)
      fi
  else
    AC_DEFINE(HAVE_RES_SEARCH, 1, [])
    AC_MSG_RESULT(yes)
  fi
  
fi

PTHREAD_LIBS=error
if test "x$MKAFS_OSTYPE" = OBSD; then
        PTHREAD_LIBS="-pthread"
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(pthread, pthread_attr_init,
                PTHREAD_LIBS="-lpthread")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(pthreads, pthread_attr_init,
                PTHREAD_LIBS="-lpthreads")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(c_r, pthread_attr_init,
                PTHREAD_LIBS="-lc_r")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_FUNC(pthread_attr_init, PTHREAD_LIBS="")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        # pthread_attr_init is a macro under HPUX 11.0 and 11.11
        AC_CHECK_LIB(pthread, pthread_attr_destroy,
                PTHREAD_LIBS="-lpthread")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_MSG_WARN(*** Unable to locate working posix thread library ***)
fi
AC_SUBST(PTHREAD_LIBS)

HOST_CPU="$host_cpu"

if test "x$with_bsd_kernel_headers" != "x"; then
	BSD_KERNEL_PATH="$with_bsd_kernel_headers"
else
	BSD_KERNEL_PATH="/usr/src/sys"
fi

if test "x$with_bsd_kernel_build" != "x"; then
	BSD_KERNEL_BUILD="$with_bsd_kernel_build"
else
	case $AFS_SYSNAME in
		*_fbsd_4?)
			BSD_KERNEL_BUILD="${BSD_KERNEL_PATH}/compile/GENERIC"
			;;
		*_fbsd_*)
			BSD_KERNEL_BUILD="${BSD_KERNEL_PATH}/${HOST_CPU}/compile/GENERIC"
			;;
	esac
fi

# Fast restart
if test "$enable_supergroups" = "yes"; then
	AC_DEFINE(SUPERGROUPS, 1, [define if you want to have support for nested pts groups])
fi

if test "$enable_fast_restart" = "yes"; then
	AC_DEFINE(FAST_RESTART, 1, [define if you want to have fast restart])
fi

if test "$enable_bitmap_later" = "yes"; then
	AC_DEFINE(BITMAP_LATER, 1, [define if you want to salvager to check bitmasks later])
fi

if test "$enable_demand_attach_fs" = "yes"; then
	AC_DEFINE(DEMAND_ATTACH_ENABLE, 1, [define if you want the demand attach fileserver])
	DEMAND_ATTACH="yes"
else
	DEMAND_ATTACH="no"
fi
AC_SUBST(DEMAND_ATTACH)

if test "$enable_unix_sockets" = "yes"; then
	AC_DEFINE(USE_UNIX_SOCKETS, 1, [define if you want to use UNIX sockets for fssync.])
	USE_UNIX_SOCKETS="yes"
else
	USE_UNIX_SOCKETS="no"
fi
AC_SUBST(USE_UNIX_SOCKETS)

if test "$enable_fast_restart" = "yes" &&
   test "$enable_demand_attach_fs" = "yes" ; then
	AC_MSG_ERROR([The Demand Attach and Fast Restart extensions are mutually exclusive.  Demand Attach fileservers automatically salvage volumes in the background, thereby making Fast Restart pointless.])
	exit 1
fi

if test "$enable_full_vos_listvol_switch" = "yes"; then
	AC_DEFINE(FULL_LISTVOL_SWITCH, 1, [define if you want to want listvol switch])
fi

if test "$enable_bos_restricted_mode" = "yes"; then
	AC_DEFINE(BOS_RESTRICTED_MODE, 1, [define if you want to want bos restricted mode])
fi

if test "$enable_bos_new_config" = "yes"; then
	AC_DEFINE(BOS_NEW_CONFIG, 1, [define if you want to enable automatic renaming of BosConfig.new to BosConfig at startup])
fi

if test "$enable_largefile_fileserver" = "yes"; then
	AC_DEFINE(AFS_LARGEFILE_ENV, 1, [define if you want large file fileserver])
fi

if test "$enable_namei_fileserver" = "yes"; then
	AC_DEFINE(AFS_NAMEI_ENV, 1, [define if you want to want namei fileserver])
fi

if test "$enable_afsdb" = "yes"; then
	LIB_AFSDB="$LIB_res_search"
	AC_DEFINE(AFS_AFSDB_ENV, 1, [define if you want to want search afsdb rr])
fi

dnl check for tivoli
AC_MSG_CHECKING(for tivoli tsm butc support)
XBSA_CFLAGS=""
if test "$enable_tivoli_tsm" = "yes"; then
	XBSADIR1=/usr/tivoli/tsm/client/api/bin/xopen
	XBSADIR2=/opt/tivoli/tsm/client/api/bin/xopen

	if test -r "$XBSADIR1/xbsa.h"; then
		XBSA_CFLAGS="-Dxbsa -I$XBSADIR1"
		AC_MSG_RESULT([yes, $XBSA_CFLAGS])
	elif test -r "$XBSADIR2/xbsa.h"; then
		XBSA_CFLAGS="-Dxbsa -I$XBSADIR2"
		AC_MSG_RESULT([yes, $XBSA_CFLAGS])
	else
		AC_MSG_RESULT([no, missing xbsa.h header file])
	fi
else
	AC_MSG_RESULT([no])
fi
AC_SUBST(XBSA_CFLAGS)

dnl checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_DIRENT
AC_CHECK_HEADERS(stdlib.h string.h unistd.h poll.h fcntl.h sys/time.h sys/file.h)
AC_CHECK_HEADERS(netinet/in.h netdb.h sys/fcntl.h sys/mnttab.h sys/mntent.h)
AC_CHECK_HEADERS(mntent.h sys/vfs.h sys/param.h sys/fs_types.h sys/fstyp.h)
AC_CHECK_HEADERS(sys/mount.h strings.h termios.h signal.h)
AC_CHECK_HEADERS(windows.h malloc.h winsock2.h direct.h io.h sys/user.h)
AC_CHECK_HEADERS(security/pam_modules.h siad.h usersec.h ucontext.h regex.h values.h)

dnl Don't build PAM on IRIX; the interface doesn't work for us.
if test "$ac_cv_header_security_pam_modules_h" = yes -a "$enable_pam" = yes; then
        case $AFS_SYSNAME in
        sgi_*)
                HAVE_PAM="no"
                ;;
        *)
	        HAVE_PAM="yes"
                ;;
        esac
else
	HAVE_PAM="no"
fi
AC_SUBST(HAVE_PAM)

if test "$enable_login" = yes; then
	BUILD_LOGIN="yes"
else
	BUILD_LOGIN="no"
fi
AC_SUBST(BUILD_LOGIN)

AC_CHECK_FUNCS(utimes random srandom getdtablesize snprintf strlcat strlcpy re_comp re_exec flock)
AC_CHECK_FUNCS(setprogname getprogname sigaction mkstemp vsnprintf strerror strcasestr)
AC_CHECK_FUNCS(setvbuf)
AC_FUNC_SETVBUF_REVERSED
AC_CHECK_FUNCS(regcomp regexec regerror)
AC_MSG_CHECKING([for POSIX regex library])
if test "$ac_cv_header_regex_h" = "yes" && \
	test "$ac_cv_func_regcomp" = "yes" && \
	test "$ac_cv_func_regexec" = "yes" && \
	test "$ac_cv_func_regerror" = "yes"; then
    AC_DEFINE(HAVE_POSIX_REGEX, 1, [define if you have POSIX regex library])
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi

AC_TYPE_SIGNAL
AC_CHECK_TYPE(ssize_t, int)
AC_CHECK_TYPE([sig_atomic_t], ,
    [AC_DEFINE([sig_atomic_t], [int],
        [Define to int if <signal.h> does not define.])],
[#include <sys/types.h>
#include <signal.h>])
AC_SIZEOF_TYPE(long)

AC_MSG_CHECKING(size of time_t)
AC_CACHE_VAL(ac_cv_sizeof_time_t,
[AC_TRY_RUN([#include <stdio.h>
#include <time.h>
main()
{
  FILE *f=fopen("conftestval", "w");
  if (!f) exit(1);
  fprintf(f, "%d\n", sizeof(time_t));
  exit(0);
}], ac_cv_sizeof_time_t=`cat conftestval`, ac_cv_sizeof_time_t=0)
])
AC_MSG_RESULT($ac_cv_sizeof_time_t)
AC_DEFINE_UNQUOTED(SIZEOF_TIME_T, $ac_cv_sizeof_time_t)

AC_CHECK_FUNCS(timegm)
AC_CHECK_FUNCS(daemon)

dnl Directory PATH handling
if test "x$enable_transarc_paths" = "xyes"  ; then 
    afsconfdir=${afsconfdir=/usr/afs/etc}
    viceetcdir=${viceetcdir=/usr/vice/etc}
    afskerneldir=${afskerneldir=${viceetcdir}}
    afssrvbindir=${afssrvbindir=/usr/afs/bin}
    afssrvsbindir=${afssrvsbindir=/usr/afs/bin}
    afssrvlibexecdir=${afssrvlibexecdir=/usr/afs/bin}
    afsdbdir=${afsdbdir=/usr/afs/db}
    afslogsdir=${afslogsdir=/usr/afs/logs}
    afslocaldir=${afslocaldir=/usr/afs/local}
    afsbackupdir=${afsbackupdir=/usr/afs/backup}
    afsbosconfigdir=${afsbosconfigdir=/usr/afs/local}
else 
    afsconfdir=${afsconfdir='${sysconfdir}/openafs/server'}
    viceetcdir=${viceetcdir='${sysconfdir}/openafs'}
    afskerneldir=${afskerneldir='${libdir}/openafs'}
    afssrvbindir=${afssrvbindir='${bindir}'}
    afssrvsbindir=${afssrvsbindir='${sbindir}'}
    afssrvlibexecdir=${afssrvlibexecdir='${libexecdir}/openafs'}
    afsdbdir=${afsdbdir='${localstatedir}/openafs/db'}
    afslogsdir=${afslogsdir='${localstatedir}/openafs/logs'}
    afslocaldir=${afslocaldir='${localstatedir}/openafs'}
    afsbackupdir=${afsbackupdir='${localstatedir}/openafs/backup'}
    afsbosconfigdir=${afsbosconfigdir='${sysconfdir}/openafs'}
fi
AC_SUBST(afsconfdir)
AC_SUBST(viceetcdir)
AC_SUBST(afskerneldir)
AC_SUBST(afssrvbindir)
AC_SUBST(afssrvsbindir)
AC_SUBST(afssrvlibexecdir)
AC_SUBST(afsdbdir)
AC_SUBST(afslogsdir)
AC_SUBST(afslocaldir)
AC_SUBST(afsbackupdir)
AC_SUBST(afsbosconfigdir)

if test "x$enable_kernel_module" = "xyes"; then
ENABLE_KERNEL_MODULE=libafs
fi

AC_SUBST(AFS_SYSNAME)
AC_SUBST(AFS_PARAM_COMMON)
AC_SUBST(ENABLE_KERNEL_MODULE)
AC_SUBST(LIB_AFSDB)
AC_SUBST(LINUX_KERNEL_PATH)
AC_SUBST(HOST_CPU)
AC_SUBST(BSD_KERNEL_PATH)
AC_SUBST(BSD_KERNEL_BUILD)
AC_SUBST(LINUX_VERSION)
AC_SUBST(MKAFS_OSTYPE)
AC_SUBST(TOP_OBJDIR)
AC_SUBST(TOP_SRCDIR)
AC_SUBST(TOP_INCDIR)
AC_SUBST(TOP_LIBDIR)
AC_SUBST(DEST)
AC_SUBST(DARWIN_INFOFILE)
AC_SUBST(IRIX_BUILD_IP35)

OPENAFS_OSCONF
OPENAFS_KRB5CONF

TOP_SRCDIR="${srcdir}/src"
dnl
dnl If we're using ./configure, need a more reasonable TOP_SRCDIR, since relative links don't work everywhere
dnl
case $TOP_SRCDIR in
        /*)
                ;;
        *)
		TOP_SRCDIR=`cd $TOP_SRCDIR; pwd`
		;;
esac

TOP_OBJDIR="${SRCDIR_PARENT}"
TOP_INCDIR="${SRCDIR_PARENT}/include"
TOP_LIBDIR="${SRCDIR_PARENT}/lib"
if test "${DEST}x" = "x"; then
        DEST="${SRCDIR_PARENT}/${AFS_SYSNAME}/dest"
fi

HELPER_SPLINT="${TOP_SRCDIR}/helper-splint.sh"
HELPER_SPLINTCFG="${TOP_SRCDIR}/splint.cfg"
AC_SUBST(HELPER_SPLINT)
AC_SUBST(HELPER_SPLINTCFG)

mkdir -p ${TOP_OBJDIR}/src/JAVA/libjafs

])
