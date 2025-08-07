
AC_DEFUN([OPENAFS_OSCONF], [

dnl defaults, override in case below as needed
RXDEBUG="-DRXDEBUG"
SHLIB_SUFFIX="so"
CCOBJ="\$(CC)"
MT_CC="\$(CC)"
XLIBS="${LIB_AFSDB} ${LIB_libintl}"
MT_LIBS='-lpthread ${XLIBS}'
XCFLAGS=

dnl debugging and optimization flag defaults
dnl Note, these are all the defaults for if debug/optimize turned on, and
dnl the arch cases below do override as needed
KERN_DBG=-g
KERN_OPTMZ=-O
DBG=-g
OPTMZ=-O
LWP_DBG=-g
NO_STRIP_BIN=
LWP_OPTMZ=-O
PAM_DBG=-g
PAM_OPTMZ=

dnl standard programs
AC_REQUIRE([AC_PROG_RANLIB])
AC_CHECK_TOOL(AS, as, [false])
AC_CHECK_PROGS(MV, mv, [false])
AC_CHECK_PROGS(RM, rm, [false])
AC_CHECK_TOOL(LD, ld, [false])
AC_CHECK_PROGS(CP, cp, [false])
AC_CHECK_PROGS(GENCAT, gencat, [false])

case $AFS_SYSNAME in
	alpha_linux_22 | alpha_linux_24 | alpha_linux_26)
		CCOBJ="\$(CC) -fPIC"
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LINKER="${CC} -shared"
		;;

	arm_linux_24 | arm_linux26 | arm64_linux26)
		CCOBJ="\$(CC) -fPIC"
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LINKER="${CC} -shared"
		;;

	hp_ux102)
		AS="/usr/ccs/bin/as"
		CC="/opt/ansic/bin/cc -Ae"
		DBM="/lib/libndbm.a"
		LD="/bin/ld"
		LEX="/opt/langtools/bin/lex"
		LWP_OPTMZ="-O"
		MT_CFLAGS='-D_POSIX_C_SOURCE=199506L'
		MT_LIBS='-L/opt/dce/lib -ldce ${XLIBS}'
		MV="/bin/mv"
		OPTMZ="-O"
		PAM_CFLAGS="+DA1.0 +z -Wl,+k"
		PAM_LIBS="/usr/lib/libpam.1"
		RANLIB="/usr/bin/ranlib"
		RM="/bin/rm"
		SHLIB_SUFFIX="sl"
		VFSCK_CFLAGS="-D_FILE64"
		XCFLAGS0="-ldld -lc -Wp,-H200000 -Wl,-a,archive -DAUTH_DBM_LOG +z -Wl,+k -D_LARGEFILE64_SOURCE"
		XCFLAGS64="${XCFLAGS0} +DA2.0W"
		XCFLAGS="${XCFLAGS0} +DA1.0"
		YACC="/opt/langtools/bin/yacc"
		SHLIB_LINKER="ld -b"
		;;

	hp_ux11*)
		AR="/usr/bin/ar"
		AS="/usr/ccs/bin/as"
		CC="/opt/ansic/bin/cc"
		DBM="/lib/libndbm.a"
		LD="/bin/ld   "
		LEX="/opt/langtools/bin/lex"
		LWP_OPTMZ="-O"
		MT_CFLAGS='-D_POSIX_C_SOURCE=199506L'
		MV="/bin/mv"
		OPTMZ="-O"
		PAM_CFLAGS="+DA1.0 +z -Wl,+k"
		PAM_LIBS="/usr/lib/libpam.1"
		RANLIB="/usr/bin/ranlib"
		RM="/bin/rm"
		SHLIB_SUFFIX="sl"
		VFSCK_CFLAGS="-I/usr/old/usr/include -D_FILE64"
		XCFLAGS0="-ldld -lc -Wp,-H200000 -Wl,-a,archive -DAUTH_DBM_LOG +z -Wl,+k -D_LARGEFILE64_SOURCE"
		XCFLAGS64="${XCFLAGS0} +DA2.0W"
		XCFLAGS="${XCFLAGS0} +DA1.0"
		YACC="/opt/langtools/bin/yacc"
		SHLIB_LINKER="ld -b"
		;;

	ia64_hpux*)
		AR="/usr/bin/ar"
		AS="/usr/ccs/bin/as"
		CC="/opt/ansic/bin/cc"
		DBM="/lib/hpux32/libndbm.so"
		LD="/bin/ld   "
		LEX="/opt/langtools/bin/lex"
		LWP_OPTMZ=""
		MT_CFLAGS='-D_POSIX_C_SOURCE=199506L'
		MV="/bin/mv"
		OPTMZ="-O"
		PAM_CFLAGS="-b +z -Wl,+k"
		PAM_LIBS="/usr/lib/hpux32/libpam.so"
		RANLIB="/usr/bin/ranlib"
		RM="/bin/rm"
		SHLIB_SUFFIX="sl"
		VFSCK_CFLAGS="-I/usr/old/usr/include -D_FILE64"
		XCFLAGS0="-ldld -lc -Wp,-H200000 -Wl,-a,archive_shared -DAUTH_DBM_LOG +z -Wl,+k -D_LARGEFILE64_SOURCE"
		XCFLAGS64="${XCFLAGS0} +DD64"
		XCFLAGS="${XCFLAGS0}"
		YACC="/opt/langtools/bin/yacc"
		SHLIB_LINKER="ld -b"
		;;

	i386_fbsd_*)
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-pipe -fPIC"
		SHLIB_LINKER="${CC} -shared"
		XCFLAGS="-pipe"
		;;

	i386_dfbsd_*)
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-pipe -fPIC"
		SHLIB_LINKER="${CC} -shared"
		XCFLAGS="-pipe"
		;;

	amd64_fbsd_*)
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_CFLAGS="-O2 -pipe -fPIC"
		SHLIB_LINKER="${CC} -shared"
		XCFLAGS="-O2 -pipe -fPIC"
		;;

	*nbsd2*|*nbsd3*|*nbsd4*|*nbsd5*|*nbsd6*|*nbsd7*)
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-pipe -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LINKER="${CC} -shared"
		XCFLAGS="-pipe"
		;;

	*nbsd15|*nbsd16)
		MT_LIBS='${XLIBS}'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-pipe -fPIC"
		SHLIB_LINKER="${CC} -shared"
		XCFLAGS="-pipe"
		;;

	ia64_linux26)
		KERN_OPTMZ=-O2
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		XCFLAGS="-G0"
		SHLIB_LINKER="${CC} -shared"
		;;

	amd64_linux*)
		CCOBJ="\$(CC) -fPIC"
		KERN_OPTMZ=-O2
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LINKER="${CC} -shared"
		;;

	ppc64_linux26|ppc64le_linux26)
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LINKER="${CC} -shared -m64"
		XCFLAGS="-m64"
		XLDFLAGS="-m64"
		ASFLAGS="-a64"
		;;

	i386_linux*)
		KERN_OPTMZ=-O2
		MT_CFLAGS='-pthread -D_REENTRANT'
		DBG=-g
		KERN_DBG=-g
		LWP_DBG=-g
		LWP_OPTMZ=-O2
		OPTMZ=-O2
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LINKER="${CC} -shared"
		;;

	i386_umlinux26)
		MT_CFLAGS='-pthread -D_REENTRANT'
		DBG=-g
		LWP_DBG=-g
		LWP_OPTMZ=-O2
		OPTMZ=-O2
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LINKER="${CC} -shared"
		;;

	*_obsd*)
		KERN_OPTMZ=-O2
		MT_CFLAGS='-pthread -D_REENTRANT'
		LWP_OPTMZ=-O2
		OPTMZ=-O2
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-pipe -fpic"
		SHLIB_CFLAGS="-fpic"
		SHLIB_LINKER="${CC} -shared"
		XCFLAGS=
		;;

	ppc_darwin_70)
		CC="cc"
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration -framework SystemConfiguration -framework IOKit -framework CoreFoundation"
		MT_CFLAGS='-D_REENTRANT'
		LWP_OPTMZ="-O2"
		REGEX_OBJ="regex.lo"
		XCFLAGS="-no-cpp-precomp"
		EXTRA_VLIBOBJS="fstab.o"
		SHLIB_LINKER="${CC} \${ARCHFLAGS} -dynamiclib"
		SHLIB_SUFFIX="dylib"
		XLIBS="${LIB_AFSDB} -framework CoreFoundation"
		;;

	*_darwin_80)
		CC="cc"
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration -framework SystemConfiguration -framework IOKit -framework CoreFoundation"
		MT_CFLAGS="-D_REENTRANT"
		KERN_OPTMZ="-Os"
		LWP_OPTMZ="-Os"
		OPTMZ="-Os"
		REGEX_OBJ="regex.lo"
		EXTRA_VLIBOBJS="fstab.o"
		SHLIB_LINKER="${CC} \${ARCHFLAGS} -dynamiclib"
		SHLIB_SUFFIX="dylib"
		RANLIB="ranlib -c"
		XLIBS="${LIB_AFSDB} -framework CoreFoundation"
		;;

	*_darwin_90)
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration -framework SystemConfiguration -framework IOKit -framework CoreFoundation"
		MT_CFLAGS="-D_REENTRANT"
		LD="cc"
		KERN_OPTMZ="-Os"
		LWP_OPTMZ="-Os"
		OPTMZ="-Os"
		REGEX_OBJ="regex.lo"
		EXTRA_VLIBOBJS="fstab.o"
		SHLIB_LINKER="${CC} \${ARCHFLAGS} -dynamiclib"
		SHLIB_SUFFIX="dylib"
		RANLIB="ranlib -c"
		XLIBS="${LIB_AFSDB} -framework CoreFoundation"
		;;

	arm_darwin_100)
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework SystemConfiguration -framework IOKit -framework CoreFoundation"
		MT_CFLAGS="-D_REENTRANT"
		MT_LIBS="${LIB_AFSDB} -framework CoreFoundation"
		LD="cc"
		KERN_OPTMZ="-Os"
		LWP_OPTMZ="-Os"
		OPTMZ="-Os"
		PAM_LIBS="-lpam"
		REGEX_OBJ="regex.lo"
		TXLIBS="-lncurses"
		EXTRA_VLIBOBJS="fstab.o"
		SHLIB_LINKER="${CC} -dynamiclib"
		SHLIB_SUFFIX="dylib"
		RANLIB="ranlib"
		XLIBS="${LIB_AFSDB} -framework CoreFoundation"
		;;

	*_darwin_100 | *_darwin_110 | *_darwin_120 | *_darwin_130 | *_darwin_140 | *_darwin_150 | *_darwin_160 | *_darwin_170 | *_darwin_180)
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration -framework SystemConfiguration -framework IOKit -framework CoreFoundation"
		MT_CFLAGS="-D_REENTRANT"
		MT_LIBS='${XLIBS}'
		LD="cc"
		KERN_OPTMZ="-Os"
		LWP_OPTMZ="-Os"
		OPTMZ="-Os"
		PAM_LIBS="-lpam"
		REGEX_OBJ="regex.lo"
		EXTRA_VLIBOBJS="fstab.o"
		SHLIB_LINKER="${CC} \${ARCHFLAGS} -dynamiclib"
		SHLIB_SUFFIX="dylib"
		RANLIB="ranlib"
		XLIBS="${LIB_AFSDB} -framework CoreFoundation"
		;;

	*_darwin_190 | *_darwin_200 | *_darwin_210 | *_darwin_220 | *_darwin_230)
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration -framework SystemConfiguration -framework IOKit -framework CoreFoundation"
		MT_CFLAGS="-D_REENTRANT"
		MT_LIBS='${XLIBS}'
		LD="cc"
		KERN_OPTMZ="-Os"
		LWP_OPTMZ="-Os"
		OPTMZ="-Os"
		PAM_LIBS="-lpam"
		REGEX_OBJ="regex.lo"
		EXTRA_VLIBOBJS="fstab.o"
		SHLIB_LINKER="${CC} \${ARCHFLAGS} -dynamiclib"
		SHLIB_SUFFIX="dylib"
		RANLIB="ranlib"
		XLIBS="${LIB_AFSDB} -framework CoreFoundation"
		;;

	ppc_linux*)
		KERN_OPTMZ=-O2
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LINKER="${CC} -shared"
		;;

	rs_aix42)
		CC="cc"
		DBG=""
		LIBSYS_AIX_EXP="afsl.exp"
		MT_CC="xlc_r"
		SHLIB_SUFFIX="o"
		XCFLAGS="-K -D_NONSTD_TYPES -D_MBI=void"
		XLIBS="${LIB_AFSDB} ${LIB_libintl} -ldl"
		SHLIB_LINKER="${MT_CC} -bM:SRE -berok"
		AIX32="yes"
		AIX64="no"
		TSM_IMPORTS="-bI:/lib/aio.exp -bI:/lib/netinet.exp -bI:/lib/sockets.exp -bI:/lib/statcmd.exp"
		TSM_LIBS="-lsys -lcsys -lc"
		;;

	rs_aix51 | rs_aix52 | rs_aix53)	
		CC="cc"
		DBG="-g"
		LIBSYS_AIX_EXP="afsl.exp"
		MT_CC="xlc_r"
		SHLIB_SUFFIX="o"
		XCFLAGS="-K -D_NONSTD_TYPES -D_MBI=void"
		XLIBS="${LIB_AFSDB} ${LIB_libintl} -ldl"
		SHLIB_LINKER="${MT_CC} -bM:SRE -berok"
		AIX32="yes"
		AIX64="yes"
		TSM_IMPORTS="-bI:/lib/aio.exp -bI:/lib/netinet.exp -bI:/lib/sockets.exp -bI:/lib/statcmd.exp"
		TSM_LIBS="-lsys -lcsys -lc"
		;;

	rs_aix61 | rs_aix7*)
		CC="cc"
		DBG="-g"
		LIBSYS_AIX_EXP="afsl.exp"
		MT_CC="xlc_r"
		SHLIB_SUFFIX="o"
		XCFLAGS="-K -D_NONSTD_TYPES -D_MBI=void"
		XLIBS="${LIB_AFSDB} ${LIB_libintl} -ldl"
		SHLIB_LINKER="${MT_CC} -bM:SRE -berok"
		AIX32="no"
		AIX64="yes"
		TSM_IMPORTS="-bI:/lib/aio.exp -bI:/lib/netinet.exp -bI:/lib/sockets.exp -bI:/lib/statcmd.exp"
		TSM_LIBS="-lsys -lcsys -lc"
		;;

	s390_linux26)
		LD="ld"
		KERN_OPTMZ=-O2
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LINKER="${CC} -shared"
		;;

	s390x_linux26)
		CCOBJ="\$(CC) -fPIC"
		LD="ld"
		KERN_OPTMZ=-O2
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		OPTMZ=-O
		XCFLAGS="-D__s390x__"
		SHLIB_LINKER="${CC} -shared"
		;;

	sgi_62)
		CC="cc"
		AFSD_LIBS="/usr/lib/libdwarf.a /usr/lib/libelf.a"
		FSINCLUDES="-I/usr/include/sys/fs"
		MT_CFLAGS='-D_SGI_MP_SOURCE'
		XCFLAGS64="-64 -mips3"
		XCFLAGS="-o32"
		XLDFLAGS64="-64"
		XLDFLAGS="-o32"
		SHLIB_LINKER="${CC} -shared"
		;;

	sgi_63)
		CC="cc"
		AFSD_LIBS="/usr/lib/libdwarf.a /usr/lib/libelf.a"
		FSINCLUDES="-I/usr/include/sys/fs"
		MT_CFLAGS='-D_SGI_MP_SOURCE'
		XCFLAGS64="-D_BSD_COMPAT -64 -mips3"
		XCFLAGS="-D_OLD_TERMIOS -D_BSD_COMPAT -o32"
		XLDFLAGS64="-64"
		XLDFLAGS="-o32"
		SHLIB_LINKER="${CC} -shared"
		;;

	sgi_64)
		CC="cc"
		AFSD_LIBS="/usr/lib32/libdwarf.a /usr/lib32/libelf.a"
		FSINCLUDES="-I/usr/include/sys/fs"
		MT_CFLAGS='-D_SGI_MP_SOURCE'
		XCFLAGS64="-D_BSD_COMPAT -64 -mips3"
		XCFLAGS="-D_OLD_TERMIOS -D_BSD_COMPAT -n32 -woff 1009,1014,1110,1116,1164,1169,1171,1174,1177,1183,1185,1204,1233,1515,1516,1548,1169,1174,1177,1196,1498,1506,1552,3201 -Wl,-woff,84,-woff,15"
		XLDFLAGS64="-64"
		XLDFLAGS="-n32"
		SHLIB_LINKER="${CC} -shared"
		;;

	sgi_65)
		AFSD_LIBS="/usr/lib32/libdwarf.a /usr/lib32/libelf.a"
		CC="/usr/bin/cc"
		FSINCLUDES="-I/usr/include/sys/fs"
		LD="/usr/bin/ld"
		MT_CFLAGS='-D_SGI_MP_SOURCE'
		XCFLAGS64="-64 -mips3"
		XCFLAGS="-n32 -mips3 -woff 1009,1014,1110,1116,1164,1171,1177,1183,1185,1204,1233,1515,1516,1548,1169,1174,1177,1196,1498,1506,1552,3201 -Wl,-woff,84,-woff,15"
		XLDFLAGS64="-64 -mips3"
		XLDFLAGS="-n32 -mips3"
		SHLIB_LINKER="${CC} -shared"
		;;

	sparc*_linux*)
		KERN_OPTMZ=-O2
		MT_CFLAGS='-pthread -D_REENTRANT'
		PAM_OPTMZ=-O2
		PAM_CFLAGS="-Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		XCFLAGS64="-m64"
		XLDFLAGS64="-m64"
		SHLIB_LINKER="${CC} -shared"
		;;

	sun4x_5*)
		CC=$SOLARISCC
		LD="/usr/ccs/bin/ld"
		MT_CFLAGS='-mt'
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		XCFLAGS64='${XCFLAGS} -m64'
		XCFLAGS="-dy -Bdynamic"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		SHLIB_LINKER="${CC} -G -dy -Bsymbolic -z text"
		LWP_OPTMZ="-g"
		;;

	sunx86_5*)
		case $AFS_SYSNAME in
			sunx86_58|sunx86_59)
				XARCHFLAGS=""
				;;
			*)
				if test "x`echo "${ARCHFLAGS}" | grep m32`" != "x" ; then
					CURRENTBUILDARCH=i386
				fi
				if test "x`echo "${ARCHFLAGS}" | grep m64`" != "x" ; then
					CURRENTBUILDARCH=amd64
				fi
				if test "x${CURRENTBUILDARCH}" = "x" ; then
					CURRENTBUILDARCH=`isainfo -k`
				fi
				if test "${CURRENTBUILDARCH}" = "amd64" ; then
					XARCHFLAGS="-m64"
				fi
				;;
		esac

		CC=$SOLARISCC
		CFLAGS="$CFLAGS ${XARCHFLAGS}"
		LD="/usr/ccs/bin/ld"
		MT_CFLAGS='-mt'
		KERN_OPTMZ="-xO3"
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		XCFLAGS0="-dy -Bdynamic"
		XCFLAGS64="${XCFLAGS0} -m64"
		XCFLAGS="${XCFLAGS0} ${XARCHFLAGS}"
		XLDFLAGS64="-m64"
		XLDFLAGS="${XARCHFLAGS}"
		ASFLAGS="${XARCHFLAGS}"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		SHLIB_LINKER="${CC} ${XARCHFLAGS} -G -dy -Bsymbolic -z text"
		;;

esac

AS_CASE([$AFS_SYSNAME],
	[*_fbsd_*],
	dnl FreeBSD 13+ no longer provides a standalone /usr/bin/as in
	dnl the base system, but we can use the compiler (clang) as an
	dnl assembler.
	[AS_IF([test x"$AS" = xfalse],
	       [AS="$CC"])],

	[*_darwin_*],
	  [OPENAFS_MACOS_KINCLUDES]
)

MT_CFLAGS="${MT_CFLAGS} -DAFS_PTHREAD_ENV"

AFS_LT_INIT

dnl if ar is not present, libtool.m4 (provided by libtool) sets AR to false
dnl if strip is not present, libtool.m4 (provided by libtool) sets STRIP to :
AS_IF([test "x$AR" = "xfalse"], [AC_MSG_ERROR([cannot find required command 'ar'])])
AS_IF([test "x$AS" = "xfalse"], [AC_MSG_ERROR([cannot find required command 'as'])])
AS_IF([test "x$MV" = "xfalse"], [AC_MSG_ERROR([cannot find required command 'mv'])])
AS_IF([test "x$RM" = "xfalse"], [AC_MSG_ERROR([cannot find required command 'rm'])])
AS_IF([test "x$LD" = "xfalse"], [AC_MSG_ERROR([cannot find required command 'ld'])])
AS_IF([test "x$CP" = "xfalse"], [AC_MSG_ERROR([cannot find required command 'cp'])])
AS_IF([test "x$GENCAT" = "xfalse"], [AC_MSG_ERROR([cannot find required command 'gencat'])])

dnl TODO - need to disable STRIP if we are doing debugging in any user space code

#
# Defaults for --enable-optimize-kernel
#
if test "x$enable_optimize_kernel" = "x" ; then
  AS_CASE([$AFS_SYSNAME],
    [sunx86_510|sunx86_511],
      dnl Somewhere around Solaris Studio 12.*, the compiler started adding SSE
      dnl instructions to optimized code, without any known way to turn it off.
      dnl To cope, this condition was added to change the default to
      dnl 'no'.
      dnl Now that we have an autoconf test to allow disabling the SSE
      dnl optimizations, it's safe to once more default to 'yes' here.
      [enable_optimize_kernel=yes],
    [enable_optimize_kernel=yes])
fi

#
# Special build targets
#
case $AFS_SYSNAME in
	sgi_6*)
		IRIX_SYS_SYSTM_H_HAS_MEM_FUNCS
		XFS_SIZE_CHECK="xfs_size_check"
		install_XFS_SIZE_CHECK='$(DESTDIR)${afssrvsbindir}/xfs_size_check'
		dest_XFS_SIZE_CHECK='$(DEST)/root.server/usr/afs/bin/xfs_size_check'
	
		AC_SUBST(XFS_SIZE_CHECK)
		AC_SUBST(install_XFS_SIZE_CHECK)
		AC_SUBST(dest_XFS_SIZE_CHECK)
	;;
	*_fbsd_*)
		if test "x$enable_debug_kernel" = "xyes"; then
			DEBUG_FLAGS=-g
			AC_SUBST(DEBUG_FLAGS)
		fi
	;;
esac

dnl Disable the default for debugging/optimization if not enabled
if test "x$enable_debug_kernel" = "xno"; then
  KERN_DBG=
fi

if test "x$enable_optimize_kernel" = "xno"; then
  KERN_OPTMZ=
fi

if test "x$enable_debug" = "xno"; then
  DBG=
  NO_STRIP_BIN=-s
fi

if test "x$enable_optimize" = "xno"; then
  OPTMZ=
fi

if test "x$enable_debug_lwp" = "xno"; then
  LWP_DBG=
fi

if test "x$enable_optimize_lwp" = "xno"; then
  LWP_OPTMZ=
fi

if test "x$enable_strip_binaries" != "xno"; then
  if test "x$enable_strip_binaries" = "xmaybe" -a "x$enable_debug" = "xyes"; then
    NO_STRIP_BIN=
  else
    NO_STRIP_BIN=-s
  fi
else
  NO_STRIP_BIN=
fi

CFLAGS_NOERROR=
CFLAGS_NOSTRICT=-fno-strict-aliasing
CFLAGS_NOUNUSED=
CFLAGS_NOOLDSTYLE=
CFLAGS_NOIMPLICIT_FALLTHROUGH=
CFLAGS_NOCAST_FUNCTION_TYPE=
CFLAGS_NODANGLING_POINTER=
XCFLAGS_NOCHECKING="$XCFLAGS"

if test "x$GCC" = "xyes"; then
  if test "x$enable_warnings" = "xyes"; then
    XCFLAGS="${XCFLAGS} -Wall -Wstrict-prototypes -Wold-style-definition -Wpointer-arith"
  fi
  if test "x$enable_checking" != "xno"; then
    XCFLAGS="${XCFLAGS} -Wall -Wstrict-prototypes -Wold-style-definition -Werror -fdiagnostics-show-option -Wpointer-arith -fno-common"
    CFLAGS_WERROR="-Werror"
    if test "x$enable_checking" != "xall"; then
      CFLAGS_NOERROR="-Wno-error"
      CFLAGS_NOUNUSED="-Wno-unused"
      CFLAGS_NOOLDSTYLE="-Wno-old-style-definition"
      AX_APPEND_COMPILE_FLAGS([-Wno-implicit-fallthrough],
			      [CFLAGS_NOIMPLICIT_FALLTHROUGH], [-Werror])
      AX_APPEND_COMPILE_FLAGS([-Wno-dangling-pointer],
			      [CFLAGS_NODANGLING_POINTER], [-Werror])
      OPENAFS_GCC_UAF_BUG_CHECK
      AC_DEFINE(IGNORE_SOME_GCC_WARNINGS, 1, [define to disable some gcc warnings in warnings-as-errors mode])
    else
      CFLAGS_NOSTRICT=
    fi
  fi
dnl If a linux kernel has CONFIG_WERROR enabled, the openafs kernel module
dnl will be built with the -Werror compiler flag even when the openafs tree is
dnl configured with --disable-checking.

dnl Provide compiler flags that can be used to disable certain warnings when the
dnl openafs tree is configued with --enable-checking or --disable-checking.
  AS_IF([test "x$enable_checking" != "xall"],
	[AX_APPEND_COMPILE_FLAGS([-Wno-cast-function-type],
				 [CFLAGS_NOCAST_FUNCTION_TYPE], [-Werror])
  ])
else
  case $AFS_SYSNAME in
    sun*_51?)
      # Solaris Studio
      warn_common="-v -errfmt=error -errtags=yes -erroff=E_ATTRIBUTE_UNKNOWN,E_END_OF_LOOP_CODE_NOT_REACHED"
      if test "x$enable_warnings" = "xyes" ; then
        XCFLAGS="${XCFLAGS} $warn_common"
      fi
      if test "x$enable_checking" != "xno" ; then
        XCFLAGS="${XCFLAGS} $warn_common -errwarn=%all"
        if test "x$enable_checking" != "xall" ; then
          CFLAGS_NOERROR="-errwarn=%none"
        fi
      fi
      ;;
  esac
fi

dnl add additional checks if compilers support the flags
AS_IF([test "x$enable_checking" != "xno"],
      [AX_APPEND_COMPILE_FLAGS([-Wimplicit-fallthrough], [XCFLAGS], [-Werror])
])

dnl horribly cheating, assuming double / is ok.
case $INSTALL in
  ./* ) 
    INSTALL="/@abs_top_srcdir@/install-sh -c"
  ;;
  *) 
  ;;
esac

INSTALL_PROGRAM="${INSTALL_PROGRAM} ${NO_STRIP_BIN}"

AC_SUBST(CCXPG2)
AC_SUBST(CCOBJ)
AC_SUBST(AFSD_LIBS)
AC_SUBST(AFSD_LDFLAGS)
AC_SUBST(AIX32)
AC_SUBST(AIX64)
AC_SUBST(AR)
AC_SUBST(AS)
AC_SUBST(ASFLAGS)
AC_SUBST(CP)
AC_SUBST(DBG)
AC_SUBST(FSINCLUDES)
AC_SUBST(KERN_DBG)
AC_SUBST(KERN_OPTMZ)
AC_SUBST(LD)
AC_SUBST(LEX)
AC_SUBST(LWP_DBG)
AC_SUBST(LWP_OPTMZ)
AC_SUBST(MT_CC)
AC_SUBST(MT_CFLAGS)
AC_SUBST(MT_LIBS)
AC_SUBST(MV)
AC_SUBST(NO_STRIP_BIN)
AC_SUBST(OPTMZ)
AC_SUBST(PAM_CFLAGS)
AC_SUBST(PAM_LIBS)
AC_SUBST(PAM_DBG)
AC_SUBST(PAM_OPTMZ)
AC_SUBST(RANLIB)
AC_SUBST(REGEX_OBJ)
AC_SUBST(RM)
AC_SUBST(RXDEBUG)
AC_SUBST(SHLIB_CFLAGS)
AC_SUBST(SHLIB_LINKER)
AC_SUBST(SHLIB_SUFFIX)
AC_SUBST(TSM_IMPORTS)
AC_SUBST(TSM_LIBS)
AC_SUBST(VFSCK_CFLAGS)
AC_SUBST(XCFLAGS)
AC_SUBST(CFLAGS_NOERROR)
AC_SUBST(CFLAGS_NOSTRICT)
AC_SUBST(CFLAGS_NOUNUSED)
AC_SUBST(CFLAGS_NOOLDSTYLE)
AC_SUBST(CFLAGS_NOIMPLICIT_FALLTHROUGH)
AC_SUBST(CFLAGS_NOCAST_FUNCTION_TYPE)
AC_SUBST(CFLAGS_NODANGLING_POINTER)
AC_SUBST(CFLAGS_WERROR)
AC_SUBST(XCFLAGS64)
AC_SUBST(XLDFLAGS)
AC_SUBST(XLDFLAGS64)
AC_SUBST(XLIBS)
AC_SUBST(YACC)

])
