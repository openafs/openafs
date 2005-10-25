
AC_DEFUN([OPENAFS_OSCONF], [

dnl defaults, override in case below as needed
XCFLAGS='${DBG} ${OPTMZ}'
SHLIB_SUFFIX="so"
CC="cc"
CCOBJ="cc"
MT_CC="cc"
XLIBS="${LIB_AFSDB}"

dnl debugging and optimization flag defaults
dnl Note, these are all the defaults for if debug/optimize turned on, and
dnl the arch cases below do not override
KERN_DBG=-g
KERN_OPTMZ=-O
DBG=-g
OPTMZ=-O
LWP_DBG=-g
LWP_OPTMZ=-O

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

dnl TODO - need to disable STRIP if we are doing debugging in any user space code

case $AFS_SYSNAME in
	alpha_dux40)
		LEX="lex"
		CSTATIC="-non_shared"
		DBG="-g3"
		MT_CFLAGS='-D_REENTRANT=1 -pthread -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-pthread -lpthread -lmach -lexc -lc"
		TXLIBS="-lcurses"
		XCFLAGS="-D_NO_PROTO -DOSF"
		SHLIB_LINKER="${CC} -all -shared -expect_unresolved \"*\""
		;;

	alpha_dux50)
		LEX="flex -l"
		DBG="-g3"
		CSTATIC="-non_shared"
		MT_CFLAGS='-D_REENTRANT=1 -pthread -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-pthread -lpthread -lmach -lexc -lc"
		TXLIBS="-lcurses"
		XCFLAGS="-D_NO_PROTO -DOSF"
		SHLIB_LINKER="${CC} -all -shared -expect_unresolved \"*\""
		;;

	alpha_dux51)
		LEX="flex -l"
		DBG="-g3"
		CSTATIC="-non_shared"
		LWP_OPTMZ="-O2"
		MT_CFLAGS='-D_REENTRANT=1 -pthread -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-pthread -lpthread -lmach -lexc -lc"
		TXLIBS="-lcurses"
		XCFLAGS="-D_NO_PROTO -DOSF"
		SHLIB_LINKER="${CC} -all -shared -expect_unresolved \"*\""
		;;

	alpha_linux_22)
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	alpha_linux_24)
		CCOBJ="${CC} -fPIC"
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	alpha_linux_26)
		CCOBJ="${CC} -fPIC"
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
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
		SHLIB_LINKER="ld -b"
		;;

	hp_ux11*)
		AR="/usr/bin/ar"
		AS="/usr/ccs/bin/as"
		CC="/opt/ansic/bin/cc"
		CCOBJ="/opt/ansic/bin/cc"
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
		#XLIBS="${LIB_AFSDB} -lnsl"
		XLIBS="${LIB_AFSDB}"
		YACC="/opt/langtools/bin/yacc"
		SHLIB_LINKER="ld -b"
		;;

	ia64_hpux*)
		AR="/usr/bin/ar"
		AS="/usr/ccs/bin/as"
		CC="/opt/ansic/bin/cc"
		CCOBJ="/opt/ansic/bin/cc"
		DBM="/lib/hpux32/libndbm.so"
		LD="/bin/ld   "
		LEX="/opt/langtools/bin/lex"
		LWP_OPTMZ=""
		MT_CC="$CC"
		MT_CFLAGS='-D_POSIX_C_SOURCE=199506L -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread"
		MV="/bin/mv"
		OPTMZ="-O"
		PAM_CFLAGS="-b +z -Wl,+k"
		PAM_LIBS="/usr/lib/hpux32/libpam.so"
		RANLIB="/usr/bin/ranlib"
		RM="/bin/rm"
		SHLIB_LDFLAGS="-b -Bsymbolic"
		SHLIB_SUFFIX="sl"
		TXLIBS="/usr/lib/hpux32/libcurses.so"
		VFSCK_CFLAGS="-I/usr/old/usr/include -D_FILE64"
		XCFLAGS0="-ldld -lc -Wp,-H200000 -Wl,-a,archive_shared -DAUTH_DBM_LOG +z -Wl,+k -D_LARGEFILE64_SOURCE"
		XCFLAGS64="${XCFLAGS0} +DD64"
		XCFLAGS="${XCFLAGS0}"
		XLIBELFA="-lelf"
		#XLIBS="${LIB_AFSDB} -lnsl"
		XLIBS="${LIB_AFSDB}"
		YACC="/opt/langtools/bin/yacc"
		SHLIB_LINKER="ld -b"
		;;

	*fbsd_*)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-pthread"
		PAM_CFLAGS="-O2 -pipe -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		SHLIB_LINKER="${MT_CC} -shared"
		TXLIBS="-lncurses"
		XCFLAGS="-O2 -pipe"
		YACC="byacc"
		;;

	*nbsd2*|*nbsd3*)
		LEX="flex -l"
		MT_CFLAGS='${XCFLAGS} -DAFS_PTHREAD_ENV -D_REENTRANT '
		MT_LIBS="-lpthread" # XXX -pthread soon
		PAM_CFLAGS="-O2 -pipe -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		SHLIB_LINKER="${MT_CC} -shared"
		TXLIBS="/usr/lib/libcurses.so"
		XCFLAGS="-O2 -pipe"
		YACC="yacc"
		;;

	*nbsd15|*nbsd16)
		LEX="flex -l"
		MT_CFLAGS='${XCFLAGS}'
		MT_LIBS=""
		PAM_CFLAGS="-O2 -pipe -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		SHLIB_LINKER="${MT_CC} -shared"
		TXLIBS="/usr/lib/libcurses.so"
		XCFLAGS="-O2 -pipe"
		YACC="bison -y"
		;;

	ia64_linux24|ia64_linux26)
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-g -O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-g -O2 -D_LARGEFILE64_SOURCE -G0"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	amd64_linux*)
		CCOBJ="${CC} -fPIC"
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-g -O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-g -O2 -D_LARGEFILE64_SOURCE"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	ppc64_linux24)
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-g -O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="/usr/lib64/libncurses.so"
		XCFLAGS="-g -O2 -D_LARGEFILE64_SOURCE"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	ppc64_linux26)
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-g -O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		SHLIB_CFLAGS="-fPIC"
		TXLIBS="-lncurses"
		XCFLAGS="-g -O2 -D_LARGEFILE64_SOURCE -fPIC"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	i386_umlinux22)
		CC="gcc -pipe"
		CCOBJ="gcc -pipe"
		MT_CC="gcc -pipe"
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	i386_linux*)
		CC="gcc -pipe"
		CCOBJ="gcc -pipe"
		MT_CC="gcc -pipe"
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		DBG=-g
		KERN_DBG=-g
		LWP_DBG=-g
		LWP_OPTMZ=-O2
		OPTMZ=-O2
		PAM_CFLAGS="-g -O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-g -O2 -D_LARGEFILE64_SOURCE"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	i386_umlinux24)
		CC="gcc -pipe"
		CCOBJ="gcc -pipe"
		MT_CC="gcc -pipe"
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		DBG=-g
		KERN_DBG=-g
		LWP_DBG=-g
		LWP_OPTMZ=-O2
		OPTMZ=-O2
		PAM_CFLAGS="-g -O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-g -O2 -D_LARGEFILE64_SOURCE"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	i386_umlinux26)
		CC="gcc -pipe"
		CCOBJ="gcc -pipe"
		MT_CC="gcc -pipe"
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		DBG=-g
		KERN_DBG=-g
		LWP_DBG=-g
		LWP_OPTMZ=-O2
		OPTMZ=-O2
		PAM_CFLAGS="-g -O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-g -O2 -D_LARGEFILE64_SOURCE"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	*_obsd*)
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-pthread"
		PAM_CFLAGS="-O2 -pipe -fpic"
		SHLIB_CFLAGS="-fpic"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		SHLIB_LINKER="${MT_CC} -shared"
		TXLIBS="/usr/lib/libcurses.a"
		XCFLAGS="-O2"
		YACC="yacc"
		;;

	parisc_linux24)
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	ppc_darwin_12)
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration"
		LEX="lex -l"
		REGEX_OBJ="regex.o"
		XCFLAGS="-traditional-cpp"
		SHLIB_LINKER="${MT_CC} -dynamiclib"
		;;

	ppc_darwin_13)
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration"
		LEX="lex -l"
		LWP_OPTMZ="-O2"
		REGEX_OBJ="regex.o"
		XCFLAGS="-no-cpp-precomp"
		SHLIB_LINKER="${MT_CC} -dynamiclib"
		;;

	ppc_darwin_14)
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration"
		LEX="lex -l"
		LWP_OPTMZ="-O2"
		REGEX_OBJ="regex.o"
		XCFLAGS="-no-cpp-precomp"
		SHLIB_LINKER="${MT_CC} -dynamiclib"
		;;

	ppc_darwin_60)
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration"
		LEX="lex -l"
		LWP_OPTMZ="-O2"
		REGEX_OBJ="regex.o"
		XCFLAGS="-no-cpp-precomp"
		TXLIBS="-lncurses"
		SHLIB_LINKER="${MT_CC} -dynamiclib"
		;;

	ppc_darwin_70)
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration"
		LEX="lex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -D_REENTRANT ${XCFLAGS}'
		KROOT=
		KINCLUDES='-I$(KROOT)/System/Library/Frameworks/Kernel.framework/Headers'
		LWP_OPTMZ="-O2"
		REGEX_OBJ="regex.o"
		XCFLAGS="-no-cpp-precomp"
		TXLIBS="-lncurses"
		EXTRA_VLIBOBJS="fstab.o"
		;;

	*_darwin_80)
		AFSD_LDFLAGS="-F/System/Library/PrivateFrameworks -framework DiskArbitration"
		LEX="lex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -D_REENTRANT ${XCFLAGS}'
		KROOT=
		KINCLUDES='-I$(KROOT)/System/Library/Frameworks/Kernel.framework/Headers'
		LWP_OPTMZ="-O2"
		REGEX_OBJ="regex.o"
		XCFLAGS="-no-cpp-precomp"
		TXLIBS="-lncurses"
		EXTRA_VLIBOBJS="fstab.o"
		SHLIB_LINKER="${MT_CC} -dynamiclib"
		SHLIB_LINKER="${MT_CC} -dynamiclib"
		;;

	ppc_linux*)
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	rs_aix42)
		DBG=""
		LEX="lex"
		LIBSYS_AIX_EXP="afsl.exp"
		MT_CC="xlc_r"
		MT_CFLAGS='-DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthreads"
		SHLIB_SUFFIX="o"
		TXLIBS="-lcurses"
		XCFLAGS="-K -D_NO_PROTO -D_NONSTD_TYPES -D_MBI=void"
		XLIBS="${LIB_AFSDB} -ldl"
		SHLIB_LINKER="${MT_CC} -bM:SRE -berok"
		AIX64="#"
		;;


	rs_aix51)
		DBG=""
		LEX="lex"
		LIBSYS_AIX_EXP="afsl.exp"
		MT_CC="xlc_r"
		MT_CFLAGS='-DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthreads"
		SHLIB_SUFFIX="o"
		TXLIBS="-lcurses"
		XCFLAGS="-K -D_NO_PROTO -D_NONSTD_TYPES -D_MBI=void"
		XLIBS="${LIB_AFSDB} -ldl"
		SHLIB_LINKER="${MT_CC} -bM:SRE -berok"
		AIX64=""
		;;

	rs_aix52)	
		DBG=""
		LEX="lex"
		LIBSYS_AIX_EXP="afsl.exp"
		MT_CC="xlc_r"
		MT_CFLAGS='-DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthreads"
		SHLIB_SUFFIX="o"
		TXLIBS="-lcurses"
		XCFLAGS="-K -D_NO_PROTO -D_NONSTD_TYPES -D_MBI=void"
		XLIBS="${LIB_AFSDB} -ldl"
		SHLIB_LINKER="${MT_CC} -bM:SRE -berok"
		AIX64=""
		;;

	rs_aix53)	
		DBG="-g"
		LEX="lex"
		LIBSYS_AIX_EXP="afsl.exp"
		MT_CC="xlc_r"
		MT_CFLAGS='-DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthreads"
		SHLIB_SUFFIX="o"
		TXLIBS="-lcurses"
		XCFLAGS="-K -D_NO_PROTO -D_NONSTD_TYPES -D_MBI=void"
		XLIBS="${LIB_AFSDB} -ldl"
		SHLIB_LINKER="${MT_CC} -bM:SRE -berok"
		AIX64=""
		;;

	s390_linux22)
		CC="gcc"
		CCOBJ="gcc"
		LD="ld"
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CC="$CC"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O -g -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	s390_linux24|s390_linux26)
		CC="gcc"
		CCOBJ="gcc"
		LD="ld"
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CC="$CC"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O -g -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	s390x_linux24|s390x_linux26)
		CC="gcc"
		CCOBJ="gcc -fPIC"
		LD="ld"
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CC="$CC"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x -Xlinker -Bsymbolic"
		TXLIBS="-lncurses"
		XCFLAGS="-O -g -D_LARGEFILE64_SOURCE -D__s390x__"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	sgi_62)
		PINSTALL_LIBS=-lmld
		AFSD_LIBS="/usr/lib/libdwarf.a /usr/lib/libelf.a"
		FSINCLUDES="-I/usr/include/sys/fs"
		LEX="lex"
		MT_CFLAGS='-D_SGI_MP_SOURCE -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread"
		TXLIBS="-lcurses"
		XCFLAGS64="-64 -mips3"
		XCFLAGS="-o32"
		XLDFLAGS64="-64"
		XLDFLAGS="-o32"
		SHLIB_LINKER="${CC} -shared"
		;;

	sgi_63)
		PINSTALL_LIBS=-lmld
		AFSD_LIBS="/usr/lib/libdwarf.a /usr/lib/libelf.a"
		FSINCLUDES="-I/usr/include/sys/fs"
		LEX="lex"
		MT_CFLAGS='-D_SGI_MP_SOURCE -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread"
		TXLIBS="-lcurses"
		XCFLAGS64="-D_BSD_COMPAT -64 -mips3"
		XCFLAGS="-D_OLD_TERMIOS -D_BSD_COMPAT -o32"
		XLDFLAGS64="-64"
		XLDFLAGS="-o32"
		SHLIB_LINKER="${CC} -shared"
		;;

	sgi_64)
		AFSD_LIBS="/usr/lib32/libdwarf.a /usr/lib32/libelf.a"
		FSINCLUDES="-I/usr/include/sys/fs"
		LEX="lex"
		MT_CFLAGS='-D_SGI_MP_SOURCE -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread"
		TXLIBS="-lcurses"
		XCFLAGS64="-D_BSD_COMPAT -64 -mips3"
		XCFLAGS="-D_OLD_TERMIOS -D_BSD_COMPAT -n32 -woff 1009,1110,1116,1164,1171,1177,1183,1185,1204,1233,1515,1516,1548,1169,1174,1177,1196,1498,1506,1552"
		XLDFLAGS64="-64"
		XLDFLAGS="-n32"
		SHLIB_LINKER="${CC} -shared"
		;;

	sgi_65)
		AFSD_LIBS="/usr/lib32/libdwarf.a /usr/lib32/libelf.a"
		CC="/usr/bin/cc"
		CCOBJ="/usr/bin/cc"
		FSINCLUDES="-I/usr/include/sys/fs"
		LD="/usr/bin/ld"
		LEX="lex"
		MT_CC="/usr/bin/cc"
		MT_CFLAGS='-D_SGI_MP_SOURCE -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread"
		TXLIBS="-lcurses"
		XCFLAGS64="-64 -mips3"
		XCFLAGS="-n32 -mips3 -woff 1009,1110,1116,1164,1171,1177,1183,1185,1204,1233,1515,1516,1548,1169,1174,1177,1196,1498,1506,1552"
		XLDFLAGS64="-64 -mips3"
		XLDFLAGS="-n32 -mips3"
		SHLIB_LINKER="${CC} -shared"
		;;

	sparc64_linux22)
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	sparc64_linux24)
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	sparc_linux22)
		KERN_OPTMZ=-O2
		LEX="flex -l"
		MT_CFLAGS='-DAFS_PTHREAD_ENV -pthread -D_REENTRANT ${XCFLAGS}'
		MT_LIBS="-lpthread"
		PAM_CFLAGS="-O2 -Dlinux -DLINUX_PAM -fPIC"
		SHLIB_LDFLAGS="-shared -Xlinker -x"
		TXLIBS="-lncurses"
		XCFLAGS="-O2 -D_LARGEFILE64_SOURCE"
		YACC="bison -y"
		SHLIB_LINKER="${MT_CC} -shared"
		;;

	sun4_413)
		CCXPG2="/usr/xpg2bin/cc"
		CC="gcc"
		CCOBJ="gcc"
		LEX="lex"
		SHLIB_CFLAGS="-PIC"
		TXLIBS="-lcurses -ltermcap"
		XCFLAGS=""
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB}"
		LD="ld"
		;;

	sun4x_55)
		CC="/opt/SUNWspro/bin/cc"
		CCOBJ="/opt/SUNWspro/bin/cc"
		LEX="lex"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		SHLIB_CFLAGS="-KPIC"
		TXLIBS="-lcurses"
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		LD="/usr/ccs/bin/ld"
		SHLIB_LINKER="${CC} -G -dy -Wl,-M\$(srcdir)/mapfile -Bsymbolic -z text"
		LWP_OPTMZ="-g"
		;;

	sun4x_56)
		CC="/opt/SUNWspro/bin/cc"
		CCOBJ="/opt/SUNWspro/bin/cc"
		LEX="lex"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
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
		SHLIB_LINKER="${CC} -G -dy -Wl,-M\$(srcdir)/mapfile -Bsymbolic -z text"
		LWP_OPTMZ="-g"
		;;

	sun4x_57)
		CC="/opt/SUNWspro/bin/cc"
		CCOBJ="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LD="/usr/ccs/bin/ld"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
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
		SHLIB_LINKER="${CC} -G -dy -Wl,-M\$(srcdir)/mapfile -Bsymbolic -z text"
		LWP_OPTMZ="-g"
		;;

	sun4x_58)
		CC="/opt/SUNWspro/bin/cc"
		CCOBJ="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LD="/usr/ccs/bin/ld"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
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
		SHLIB_LINKER="${CC} -G -dy -Wl,-M\$(srcdir)/mapfile -Bsymbolic -z text"
		LWP_OPTMZ="-g"
		;;

	sun4x_59)
		CC="/opt/SUNWspro/bin/cc"
		CCOBJ="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LD="/usr/ccs/bin/ld"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
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
		SHLIB_LINKER="${CC} -G -dy -Wl,-M\$(srcdir)/mapfile -Bsymbolic -z text"
		LWP_OPTMZ="-g"
		;;

	sun4x_510)
		CC="/opt/SUNWspro/bin/cc"
		CCOBJ="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LD="/usr/ccs/bin/ld"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
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
		SHLIB_LINKER="${CC} -G -dy -Wl,-M\$(srcdir)/mapfile -Bsymbolic -z text"
		LWP_OPTMZ="-g"
		;;

	sunx86_57)
		CC="/opt/SUNWspro/bin/cc"
		CCOBJ="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LD="/usr/ccs/bin/ld"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		SHLIB_LDFLAGS="-G -Bsymbolic"
		TXLIBS="-lcurses"
		XCFLAGS64='${XCFLAGS} -xarch=amd64'
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		SHLIB_LINKER="${CC} -G -dy -Wl,-M\$(srcdir)/mapfile -Bsymbolic -z text"
		;;

	sunx86_58)
		CC="/opt/SUNWspro/bin/cc"
		CCOBJ="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LD="/usr/ccs/bin/ld"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		SHLIB_LDFLAGS="-G -Bsymbolic"
		TXLIBS="-lcurses"
		XCFLAGS64='${XCFLAGS} -xarch=amd64'
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		SHLIB_LINKER="${CC} -G -dy -Wl,-M\$(srcdir)/mapfile -Bsymbolic -z text"
		;;

	sunx86_59)
		CC="/opt/SUNWspro/bin/cc"
		CCOBJ="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LD="/usr/ccs/bin/ld"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		SHLIB_LDFLAGS="-G -Bsymbolic"
		TXLIBS="-lcurses"
		XCFLAGS64='${XCFLAGS} -xarch=amd64'
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		SHLIB_LINKER="${CC} -G -dy -Wl,-M\$(srcdir)/mapfile -Bsymbolic -z text"
		;;

	sunx86_510)
		CC="/opt/SUNWspro/bin/cc"
		CCOBJ="/opt/SUNWspro/bin/cc"
		LEX="lex"
		LD="/usr/ccs/bin/ld"
		MT_CC="/opt/SUNWspro/bin/cc"
		MT_CFLAGS='-mt -DAFS_PTHREAD_ENV ${XCFLAGS}'
		MT_LIBS="-lpthread -lsocket"
		PAM_CFLAGS="-KPIC"
		PAM_LIBS="-lc -lpam -lsocket -lnsl -lm"
		SHLIB_CFLAGS="-KPIC"
		SHLIB_LDFLAGS="-G -Bsymbolic"
		TXLIBS="-lcurses"
		XCFLAGS64='${XCFLAGS} -xarch=amd64'
		XCFLAGS="-dy -Bdynamic"
		XLIBELFA="-lelf"
		XLIBKVM="-lkvm"
		XLIBS="${LIB_AFSDB} -lsocket -lnsl -lintl -ldl"
		SHLIB_LINKER="${CC} -G -dy -Wl,-M\$(srcdir)/mapfile -Bsymbolic -z text"
		;;
esac

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
	
	sun4x_*|sunx86_*)
		FS_CONV_SOL26="fs_conv_sol26"
		install_FS_CONV_SOL26='$(DESTDIR)${afssrvsbindir}/fs_conv_sol26'
		dest_FS_CONV_SOL26='$(DEST)/root.server/usr/afs/bin/fs_conv_sol26'

		AC_SUBST(FS_CONV_SOL26)
		AC_SUBST(install_FS_CONV_SOL26)
		AC_SUBST(dest_FS_CONV_SOL26)
	;;

	alpha_dux*)
		FS_CONV_OSF40D="fs_conv_dux40D"
		install_FS_CONV_OSF40D='$(DESTDIR)${afssrvsbindir}/fs_conv_dux40D'
		dest_FS_CONV_OSF40D='$(DEST)/root.server/usr/afs/bin/fs_conv_dux40D'

		AC_SUBST(FS_CONV_OSF40D)
		AC_SUBST(install_FS_CONV_OSF40D)
		AC_SUBST(dest_FS_CONV_OSF40D)
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

AC_SUBST(CCXPG2)
AC_SUBST(CCOBJ)
AC_SUBST(AFSD_LIBS)
AC_SUBST(AFSD_LDFLAGS)
AC_SUBST(AIX64)
AC_SUBST(AR)
AC_SUBST(AS)
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
AC_SUBST(OPTMZ)
AC_SUBST(PAM_CFLAGS)
AC_SUBST(PAM_LIBS)
AC_SUBST(PINSTALL_LIBS)
AC_SUBST(RANLIB)
AC_SUBST(REGEX_OBJ)
AC_SUBST(RM)
AC_SUBST(SHLIB_CFLAGS)
AC_SUBST(SHLIB_LDFLAGS)
AC_SUBST(SHLIB_LINKER)
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
