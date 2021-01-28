AC_DEFUN([OPENAFS_SYSNAME],[
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
                x86_64-*-openbsd?.?)
                        v=${host#*openbsd}
                        vM=${v%.*}
                        vm=${v#*.}
                        AFS_SYSNAME="amd64_obsd${vM}${vm}"
                        ;;
                i?86-*-freebsd*.*)
                        v=${host#*freebsd}
                        vM=${v%.*}
                        vm=${v#*.}
                        AFS_SYSNAME="i386_fbsd_${vM}${vm}"
                        ;;
                x86_64-*-freebsd*.*)
                        v=${host#*freebsd}
                        vM=${v%.*}
                        vm=${v#*.}
                        AFS_SYSNAME="amd64_fbsd_${vM}${vm}"
                        ;;
                i386-*-dragonfly?.*)
                        v=${host#*dragonfly}
                        vM=${v%.*}
                        vm=${v#*.}
                        AFS_SYSNAME="i386_dfbsd_${vM}${vm}"
                        ;;
                i?86-*-netbsd*1.6[[M-Z]]*)
                        AFS_SYSNAME="i386_nbsd20"
                        ;;
                powerpc-*-netbsd*1.6[[M-Z]]*)
                        AFS_SYSNAME="ppc_nbsd20"
                        ;;
                *-*-netbsd*)
                        arch=${host%%-unknown*}
                        arch=$(echo $arch |sed -e 's/x86_64/amd64/g' \
                                               -e 's/powerpc/ppc/g')
                        v=${host#*netbsd}
                        v=${v#*aout}
                        v=${v#*ecoff}
                        v=${v#*elf}
                        vM=${v%%.*}
                        vM=${vM%.*}
                        v=${v#*.}
                        vm=${v%%.*}
                        vm=${vm%.*}
                        vm=${vm%%[[_A-Z]]*}
                        if test $vm -eq 99 ; then
                                vM=$((vM+1))
                        fi
                        if test $vM -gt 1 ; then
                                vm=0
                        fi
                        AFS_SYSNAME="${arch}_nbsd${vM}${vm}"
                        ;;
                hppa*-hp-hpux11.0*)
                        AFS_SYSNAME="hp_ux110"
                        ;;
                hppa*-hp-hpux11.11)
                        AFS_SYSNAME="hp_ux11i"
                        ;;
                hppa*-hp-hpux11.23)
                        AFS_SYSNAME="hp_ux1123"
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
                powerpc-apple-darwin7*)
                        AFS_SYSNAME="ppc_darwin_70"
                        OSXSDK="macosx10.3"
                        ;;
                powerpc-apple-darwin8.*)
                        AFS_SYSNAME="ppc_darwin_80"
                        OSXSDK="macosx10.4"
                        ;;
                i386-apple-darwin8.*)
                        AFS_SYSNAME="x86_darwin_80"
                        OSXSDK="macosx10.4"
                        ;;
                powerpc-apple-darwin9.*)
                        AFS_SYSNAME="ppc_darwin_90"
                        OSXSDK="macosx10.5"
                        ;;
                i386-apple-darwin9.*)
                        AFS_SYSNAME="x86_darwin_90"
                        OSXSDK="macosx10.5"
                        ;;
                i?86-apple-darwin10.*)
                        AFS_SYSNAME="x86_darwin_100"
                        OSXSDK="macosx10.6"
                        ;;
                x86_64-apple-darwin10.*)
                        AFS_SYSNAME="x86_darwin_100"
                        OSXSDK="macosx10.6"
                        ;;
                arm-apple-darwin10.*)
                        AFS_SYSNAME="arm_darwin_100"
                        OSXSDK="iphoneos4.0"
                        ;;
                x86_64-apple-darwin11.*)
                        AFS_SYSNAME="x86_darwin_110"
                        OSXSDK="macosx10.7"
                        ;;
                i?86-apple-darwin11.*)
                        AFS_SYSNAME="x86_darwin_110"
                        OSXSDK="macosx10.7"
                        ;;
                x86_64-apple-darwin12.*)
                        AFS_SYSNAME="x86_darwin_120"
                        OSXSDK="macosx10.8"
                        ;;
                i?86-apple-darwin12.*)
                        AFS_SYSNAME="x86_darwin_120"
                        OSXSDK="macosx10.8"
                        ;;
                x86_64-apple-darwin13.*)
                        AFS_SYSNAME="x86_darwin_130"
                        OSXSDK="macosx10.9"
                        ;;
                i?86-apple-darwin13.*)
                        AFS_SYSNAME="x86_darwin_130"
                        OSXSDK="macosx10.9"
                        ;;
                x86_64-apple-darwin14.*)
                        AFS_SYSNAME="x86_darwin_140"
                        OSXSDK="macosx10.10"
                        ;;
                i?86-apple-darwin14.*)
                        AFS_SYSNAME="x86_darwin_140"
                        OSXSDK="macosx10.10"
                        ;;
                x86_64-apple-darwin15.*)
                        AFS_SYSNAME="x86_darwin_150"
                        OSXSDK="macosx10.11"
                        ;;
                i?86-apple-darwin15.*)
                        AFS_SYSNAME="x86_darwin_150"
                        OSXSDK="macosx10.11"
                        ;;
                x86_64-apple-darwin16.*)
                        AFS_SYSNAME="x86_darwin_160"
                        OSXSDK="macosx10.12"
                        ;;
                i?86-apple-darwin16.*)
                        AFS_SYSNAME="x86_darwin_160"
                        OSXSDK="macosx10.12"
                        ;;
                x86_64-apple-darwin17.*)
                        AFS_SYSNAME="x86_darwin_170"
                        OSXSDK="macosx10.13"
                        ;;
                i?86-apple-darwin17.*)
                        AFS_SYSNAME="x86_darwin_170"
                        OSXSDK="macosx10.13"
                        ;;
		x86_64-apple-darwin18.*)
			AFS_SYSNAME="x86_darwin_180"
			OSXSDK="macosx10.14"
			;;
		i?86-apple-darwin18.*)
			AFS_SYSNAME="x86_darwin_180"
			OSXSDK="macosx10.14"
			;;
		x86_64-apple-darwin19.*)
			AFS_SYSNAME="x86_darwin_190"
			OSXSDK="macosx10.15"
			;;
		i?86-apple-darwin19.*)
			AFS_SYSNAME="x86_darwin_190"
			OSXSDK="macosx10.15"
			;;
		x86_64-apple-darwin20.*)
			AFS_SYSNAME="x86_darwin_200"
			OSXSDK="macosx11.0"
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
                mips-sgi-irix6.5)
                        AFS_SYSNAME="sgi_65"
                        enable_pam="no"
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
                powerpc64le-*-linux*)
                        AFS_SYSNAME="ppc64le_linuxXX"
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
                arm*-linux*)
                        AFS_SYSNAME="arm_linuxXX"
                        ;;
                aarch64*-linux*)
                        AFS_SYSNAME="arm64_linuxXX"
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
                power*-ibm-aix6.1*)
                        AFS_SYSNAME="rs_aix61"
                        enable_pam="no"
                        ;;
                x86_64-*-linux-gnu)
                        AFS_SYSNAME="amd64_linuxXX"
                        enable_pam="yes"
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
                        if test "x${AFS_SYSKVERS}" = "x24" ||
                                test "x${AFS_SYSKVERS}" = "x22"; then
                            AC_MSG_ERROR([Linux 2.4.x and older are no longer supported by OpenAFS.  Please use an OpenAFS 1.6.x release on those systems.])
                        fi
                        _AFS_SYSNAME=`echo $AFS_SYSNAME|sed s/XX\$/$AFS_SYSKVERS/`
                        AFS_SYSNAME="$_AFS_SYSNAME"
                        AC_TRY_KBUILD(
                         [],
                         [#ifndef CONFIG_USERMODE
                          #error not UML
                          #endif],
                         ac_cv_linux_is_uml=yes,)
                        if test "${ac_cv_linux_is_uml}" = yes; then
                         _AFS_SYSNAME=`echo $AFS_SYSNAME|sed s/linux/umlinux/`
                        fi
                        AFS_SYSNAME="$_AFS_SYSNAME"
                        ;;
        esac
        AC_MSG_RESULT($AFS_SYSNAME)
fi

case $AFS_SYSNAME in
        *_darwin*)
                AC_CHECK_HEADERS(crt_externs.h)
                DARWIN_PLIST=src/libafs/afs.${AFS_SYSNAME}.plist
                DARWIN_INFOFILE=afs.${AFS_SYSNAME}.plist
                AC_SUBST(OSXSDK)
                ;;
esac

dnl Some hosts have a separate common param file they should include.  Figure
dnl out if we're on one of them now that we know the sysname.
case $AFS_SYSNAME in
    *_nbsd15)   AFS_PARAM_COMMON=param.nbsd15.h  ;;
    *_nbsd16)   AFS_PARAM_COMMON=param.nbsd16.h  ;;
    *_nbsd20)   AFS_PARAM_COMMON=param.nbsd20.h  ;;
    *_nbsd21)   AFS_PARAM_COMMON=param.nbsd21.h  ;;
    *_nbsd30)   AFS_PARAM_COMMON=param.nbsd30.h  ;;
    *_nbsd40)   AFS_PARAM_COMMON=param.nbsd40.h  ;;
    *_nbsd50)   AFS_PARAM_COMMON=param.nbsd50.h  ;;
    *_nbsd60)   AFS_PARAM_COMMON=param.nbsd60.h  ;;
    *_nbsd70)   AFS_PARAM_COMMON=param.nbsd70.h  ;;
    *_obsd31)   AFS_PARAM_COMMON=param.obsd31.h  ;;
    *_obsd32)   AFS_PARAM_COMMON=param.obsd32.h  ;;
    *_obsd33)   AFS_PARAM_COMMON=param.obsd33.h  ;;
    *_obsd34)   AFS_PARAM_COMMON=param.obsd34.h  ;;
    *_obsd35)   AFS_PARAM_COMMON=param.obsd35.h  ;;
    *_obsd36)   AFS_PARAM_COMMON=param.obsd36.h  ;;
    *_obsd37)   AFS_PARAM_COMMON=param.obsd37.h  ;;
    *_obsd38)   AFS_PARAM_COMMON=param.obsd38.h  ;;
    *_obsd39)   AFS_PARAM_COMMON=param.obsd39.h  ;;
    *_obsd40)   AFS_PARAM_COMMON=param.obsd40.h  ;;
    *_obsd41)   AFS_PARAM_COMMON=param.obsd41.h  ;;
    *_obsd42)   AFS_PARAM_COMMON=param.obsd42.h  ;;
    *_obsd43)   AFS_PARAM_COMMON=param.obsd43.h  ;;
    *_obsd44)   AFS_PARAM_COMMON=param.obsd44.h  ;;
    *_obsd45)   AFS_PARAM_COMMON=param.obsd45.h  ;;
    *_obsd46)   AFS_PARAM_COMMON=param.obsd46.h  ;;
    *_obsd47)   AFS_PARAM_COMMON=param.obsd47.h  ;;
    *_obsd48)   AFS_PARAM_COMMON=param.obsd48.h  ;;
    *_obsd49)   AFS_PARAM_COMMON=param.obsd49.h  ;;
    *_obsd50)   AFS_PARAM_COMMON=param.obsd50.h  ;;
    *_obsd51)   AFS_PARAM_COMMON=param.obsd51.h  ;;
    *_obsd52)   AFS_PARAM_COMMON=param.obsd52.h  ;;
    *_obsd53)   AFS_PARAM_COMMON=param.obsd53.h  ;;
    *_obsd54)   AFS_PARAM_COMMON=param.obsd54.h  ;;
    *_linux26)  AFS_PARAM_COMMON=param.linux26.h ;;
# Linux alpha adds an extra underscore for no good reason.
    *_linux_26) AFS_PARAM_COMMON=param.linux26.h ;;
    *_fbsd_*)   AFS_PARAM_COMMON=param.generic_fbsd.h ;;
esac
])
