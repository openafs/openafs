
AC_DEFUN(OPENAFS_OSCONF, [

dnl defaults, override in case below as needed
DBG="-g"
OPTMZ="-O2"
XCFLAGS="-O2"
SHLIB_SUFFIX="so"
CC="cc"
MT_CC="cc"
XLIBS="${LIB_AFSDB}"

dnl standard programs
AC_PROG_RANLIB
AC_CHECK_PROGS(AS, as, [${am_missing_run}as])
AC_CHECK_PROGS(AR, ar, [${am_missing_run}ar])
AC_CHECK_PROGS(MV, mv, [${am_missing_run}mv])
AC_CHECK_PROGS(RM, rm, [${am_missing_run}rm])
AC_CHECK_PROGS(LD, ld, [${am_missing_run}ld])
AC_CHECK_PROGS(CP, cp, [${am_missing_run}cp])
AC_CHECK_PROGS(STRIP, strip, [${am_missing_run}strip])
AC_CHECK_PROGS(LORDER, lorder, [${am_missing_run}lorder])

case $AFS_SYSNAME in
	alpha_dux40)
		CSTATIC="-non_shared"
		LWP_OPTMZ="-O2"
		MT_CFLAGS='-D_REENTRANT=1 -pthread -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-pthread -lpthread -lmach -lexc -lc"
		TXLIBS="-lcurses"
		XCFLAGS="-D_NO_PROTO -DOSF"
		;;

	alpha_dux50)
		CSTATIC="-non_shared"
		LWP_OPTMZ="-O2"
		MT_CFLAGS='-D_REENTRANT=1 -pthread -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-pthread -lpthread -lmach -lexc -lc"
		TXLIBS="-lcurses"
		XCFLAGS="-D_NO_PROTO -DOSF"
		;;

	alpha_linux_22)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	alpha_linux_24)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	hp_ux102)
		AS="/usr/ccs/bin/as"
		CC="/opt/ansic/bin/cc -Ae"
		DBM="/lib/libndbm.a"
		LD="/bin/ld"
		LEX="/opt/langtools/bin/lex"
		LWP_OPTMZ="-O"
		MT_CC="/opt/ansic/bin/cc -Ae"
		MT_CFLAGS='-D_POSIX_C_SOURCE=199506L -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-L/opt/dce/lib -ldce"
		MV="/bin/mv"
		OPTMZ="-O"
		PAM_CFLAGS="+DA1.0 +z -Wl,+k"
		PAM_LIBS="/usr/lib/libpam.1"
		RANLIB="/usr/bin/ranlib"
		RM="/bin/rm"
		SHLIB_LDFLAGS="-b -Bsymbolic"
		SHLIB_SUFFIX="sl"
		TXLIBS="/usr/lib/libHcurses.a"
		VFSCK_CFLAGS="-D_FILE64"
		XCFLAGS0="-ldld -lc -Wp,-H200000 -Wl,-a,archive -DAUTH_DBM_LOG +z -Wl,+k -D_LARGEFILE64_SOURCE"
		XCFLAGS64="${XCFLAGS0} +DA2.0W"
		XCFLAGS="${XCFLAGS0} +DA1.0"
		YACC="/opt/langtools/bin/yacc"
		;;

	hp_ux110)
		AS="/usr/ccs/bin/as"
		CC="/opt/ansic/bin/cc"
		DBM="/lib/libndbm.a"
		LD="/bin/ld   "
		LEX="/opt/langtools/bin/lex"
		LWP_OPTMZ="-O"
		MT_CC="$CC"
		MT_CFLAGS='-D_POSIX_C_SOURCE=199506L -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread"
		MV="/bin/mv"
		OPTMZ="-O"
		PAM_CFLAGS="+DA1.0 +z -Wl,+k"
		PAM_LIBS="/usr/lib/libpam.1"
		RANLIB="/usr/bin/ranlib"
		RM="/bin/rm"
		SHLIB_LDFLAGS="-b -Bsymbolic"
		SHLIB_SUFFIX="sl"
		TXLIBS="/usr/lib/libHcurses.a"
		VFSCK_CFLAGS="-I/usr/old/usr/include -D_FILE64"
		XCFLAGS0="-ldld -lc -Wp,-H200000 -Wl,-a,archive -DAUTH_DBM_LOG +z -Wl,+k -D_LARGEFILE64_SOURCE"
		XCFLAGS64="${XCFLAGS0} +DA2.0W"
		XCFLAGS="${XCFLAGS0} +DA1.0"
		XLIBELFA="-lelf"
		XLIBS="${LIB_AFSDB} -lnsl"
		YACC="/opt/langtools/bin/yacc"
		;;

	i386_fbsd_42)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-pthread"
		PAM_CFLAGS="-O2 -pipe -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -pipe"
		XLIBS="${LIB_AFSDB} -lcompat"
		YACC="bison -y"
		;;

	*nbsd15)
		LEX="flex -l"
		MT_CFLAGS='${XCFLAGS}'
		MT_LIBS=""
		PAM_CFLAGS="-O2 -pipe -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libcurses.so"
		XCFLAGS="-O2 -pipe"
		XLIBS="${LIB_AFSDB} -lcompat"
		YACC="bison -y"
		;;

	i386_linux22)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	i386_linux24)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		;;
	
	i386_obsd29)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-pthread"
		PAM_CFLAGS="-O2 -pipe -fpic"
		SHLIB_CFLAGS="-fpic"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libcurses.a"
		XCFLAGS="-O2"
		XLIBS="${LIB_AFSDB} -lcompat"
		YACC="yacc"
		;;

	parisc_linux24)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	ppc_darwin_12)
		LEX="lex -l"
		LWP_OPTMZ="-g"
		OPTMZ="-g"
		REGEX_OBJ="regex.o"
		XCFLAGS="-traditional-cpp"
		;;

	ppc_darwin_13)
		LEX="lex -l"
		LWP_OPTMZ="-O2"
		REGEX_OBJ="regex.o"
		XCFLAGS="-no-cpp-precomp"
		;;

	ppc_darwin_14)
		LEX="lex -l"
		LWP_OPTMZ="-O2"
		REGEX_OBJ="regex.o"
		XCFLAGS="-no-cpp-precomp"
		;;

	ppc_linux22)
		INSTALL="install"
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	ppc_linux24)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	rs_aix42)
		DBG=""
		LEX="lex"
		LIBSYS_AIX_EXP="afsl.exp"
		LWP_OPTMZ="-O"
		MT_CC="xlc_r"
		MT_CFLAGS='-DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthreads"
		OPTMZ="-O"
		SHLIB_SUFFIX="o"
		TXLIBS="-lcurses"
		XCFLAGS="-K -D_NO_PROTO -D_NONSTD_TYPES -D_MBI=void"
		XLIBS="${LIB_AFSDB} -ldl"
		;;

	s390_linux22)
		CC="gcc"
		LD="gcc"
		LEX="flex -l"
		MT_CC="$CC"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		OPTMZ="-O"
		PAM_CFLAGS="-O -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O -g -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	s390_linux24)
		CC="gcc"
		LD="gcc"
		LEX="flex -l"
		MT_CC="$CC"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		OPTMZ="-O"
		PAM_CFLAGS="-O -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O -g -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	sgi_62)
		FSINCLUDES="-I/usr/include/sys/fs"
		LEX="lex"
		LWP_OPTMZ="-O"
		MT_CFLAGS='-D_SGI_MP_SOURCE -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread"
		OPTMZ="-O"
		TXLIBS="-lcurses"
		XCFLAGS64="-64 -mips3"
		XCFLAGS="-o32"
		XLDFLAGS64="-64"
		XLDFLAGS="-o32"
		;;

	sgi_63)
		FSINCLUDES="-I/usr/include/sys/fs"
		LEX="lex"
		LWP_OPTMZ="-O"
		MT_CFLAGS='-D_SGI_MP_SOURCE -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread"
		OPTMZ="-O"
		TXLIBS="-lcurses"
		XCFLAGS64="-D_BSD_COMPAT -64 -mips3"
		XCFLAGS="-D_OLD_TERMIOS -D_BSD_COMPAT -o32"
		XLDFLAGS64="-64"
		XLDFLAGS="-o32"
		;;

	sgi_64)
		FSINCLUDES="-I/usr/include/sys/fs"
		LEX="lex"
		LWP_OPTMZ="-O"
		MT_CFLAGS='-D_SGI_MP_SOURCE -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread"
		OPTMZ="-O"
		TXLIBS="-lcurses"
		XCFLAGS64="-D_BSD_COMPAT -64 -mips3"
		XCFLAGS="-D_OLD_TERMIOS -D_BSD_COMPAT -n32 -woff 1009,1110,1116,1164,1171,1177,1183,1185,1204,1233,1515,1516,1548,1169,1174,1177,1196,1498,1506,1552"
		XLDFLAGS64="-64"
		XLDFLAGS="-n32"
		;;

	sgi_65)
		CC="/usr/bin/cc"
		FSINCLUDES="-I/usr/include/sys/fs"
		LD="/usr/bin/ld"
		LEX="lex"
		MT_CC="/usr/bin/cc"
		MT_CFLAGS='-D_SGI_MP_SOURCE -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread"
		OPTMZ="-O"
		TXLIBS="-lcurses"
		XCFLAGS64="-64 -mips3"
		XCFLAGS="-n32 -mips3 -woff 1009,1110,1116,1164,1171,1177,1183,1185,1204,1233,1515,1516,1548,1169,1174,1177,1196,1498,1506,1552"
		XLDFLAGS64="-64 -mips3"
		XLDFLAGS="-n32 -mips3"
		;;

	sparc64_linux22)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	sparc64_linux24)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	sparc_linux22)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib/libncurses.so"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		;;

	sun4x_55)
		CC="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LWP_OPTMZ="-g"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		OPTMZ="-O"
		SHLIB_CFLAGS="-KPIC"
		TXLIBS="-lcurses"
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		LD="/usr/ccs/bin/ld"
		;;

	sun4x_56)
		CC="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LWP_OPTMZ="-g"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		OPTMZ="-O"
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		SHLIB_LDFLAGS="-G -Bsymbolic"
		TXLIBS="-L/usr/ccs/lib -lcurses"
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		LD="/usr/ccs/bin/ld"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		;;

	sun4x_57)
		CC="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LD="/usr/ccs/bin/ld"
		LWP_OPTMZ="-g"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		OPTMZ="-O"
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		SHLIB_LDFLAGS="-G -Bsymbolic"
		TXLIBS="-lcurses"
		XCFLAGS64='${XCFLAGS} -xarch=v9'
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		;;

	sun4x_58)
		CC="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LD="/usr/ccs/bin/ld"
		LWP_OPTMZ="-g"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		OPTMZ="-O"
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		SHLIB_LDFLAGS="-G -Bsymbolic"
		TXLIBS="-lcurses"
		XCFLAGS64='${XCFLAGS} -xarch=v9'
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		;;

	sun4x_59)
		CC="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LWP_OPTMZ="-g"
		LD="/usr/ccs/bin/ld"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		OPTMZ="-O"
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		SHLIB_LDFLAGS="-G -Bsymbolic"
		TXLIBS="-lcurses"
		XCFLAGS64='${XCFLAGS} -xarch=v9'
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		;;

	sunx86_58)
		CC="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LWP_OPTMZ="-g"
		LD="/usr/ccs/bin/ld"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		OPTMZ="-O"
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		SHLIB_LDFLAGS="-G -Bsymbolic"
		TXLIBS="-lcurses"
		XCFLAGS64='${XCFLAGS} -xarch=v9'
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		;;


esac

#
# Special build targets
#
case $AFS_SYSNAME in
	sgi_6*)
		XFS_SIZE_CHECK="xfs_size_check"
		install_XFS_SIZE_CHECK='$(DESTDIR)${afssrvsbindir}/xfs_size_check'
		dest_XFS_SIZE_CHECK='$(DEST)/root.server/usr/afs/bin/xfs_size_check'
	
		AC_SUBST(XFS_SIZE_CHECK)
		AC_SUBST(install_XFS_SIZE_CHECK)
		AC_SUBST(dest_XFS_SIZE_CHECK)
	;;
	
	sun4x_*|sunx86_*)
		FS_CONV_SOL26="fs_conv_sol26"
		install_FS_CONV_SOL26='$(DESTDIR)${afssrvsbindir}/fs_conv_sol26'
		dest_FS_CONV_SOL26='$(DEST)/root.server/usr/afs/bin/fs_conv_sol26'

		AC_SUBST(FS_CONV_SOL26)
		AC_SUBST(install_FS_CONV_SOL26)
		AC_SUBST(dest_FS_CONV_SOL26)
	;;

	alpha_dux*)
		FS_CONV_OSF40D="fs_conv_dux40d"
		install_FS_CONV_OSF40D='$(DESTDIR)${afssrvsbindir}/fs_conv_dux40d'
		dest_FS_CONV_OSF40D='$(DEST)/root.server/usr/afs/bin/fs_conv_sdux40d'

		AC_SUBST(FS_CONV_OSF40D)
		AC_SUBST(install_FS_CONV_OSF40D)
		AC_SUBST(dest_FS_CONV_OSF40D)
	;;
esac


AC_SUBST(AR)
AC_SUBST(AS)
AC_SUBST(CP)
AC_SUBST(DBG)
AC_SUBST(FSINCLUDES)
AC_SUBST(LD)
AC_SUBST(LEX)
AC_SUBST(LWP_OPTMZ)
AC_SUBST(MT_CC)
AC_SUBST(MT_CFLAGS)
AC_SUBST(MT_LIBS)
AC_SUBST(MV)
AC_SUBST(OPTMZ)
AC_SUBST(PAM_CFLAGS)
AC_SUBST(PAM_LIBS)
AC_SUBST(RANLIB)
AC_SUBST(REGEX_OBJ)
AC_SUBST(RM)
AC_SUBST(SHLIB_CFLAGS)
AC_SUBST(SHLIB_LDFLAGS)
AC_SUBST(SHLIB_SUFFIX)
AC_SUBST(TXLIBS)
AC_SUBST(VFSCK_CFLAGS)
AC_SUBST(XCFLAGS)
AC_SUBST(XCFLAGS64)
AC_SUBST(XLDFLAGS)
AC_SUBST(XLDFLAGS64)
AC_SUBST(XLIBELFA)
AC_SUBST(XLIBKVM)
AC_SUBST(XLIBS)
AC_SUBST(YACC)


])
