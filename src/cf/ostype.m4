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
                    OPENAFS_LINUX_KERNEL_PATH
                fi
                AC_MSG_RESULT(linux)
                OPENAFS_LINUX_GUESS_VERSION
                ;;
        *-solaris*)
                MKAFS_OSTYPE=SOLARIS
                AC_MSG_RESULT(sun4)
                OPENAFS_SOLARIS_OSTYPE
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
