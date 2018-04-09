
AC_DEFUN([OPENAFS_OPTIONS],[

dnl System identity.
AC_ARG_WITH([afs-sysname],
    [AS_HELP_STRING([--with-afs-sysname=sys], [use sys for the afs sysname])
])

dnl General feature options.
AC_ARG_ENABLE([namei-fileserver],
    [AS_HELP_STRING([--enable-namei-fileserver],
        [force compilation of namei fileserver in preference to inode
         fileserver])],
    [],
    [enable_namei_fileserver="default"])
AC_ARG_ENABLE([supergroups],
    [AS_HELP_STRING([--enable-supergroups],
        [enable support for nested pts groups])],
    [],
    [enable_supergroups="no"])
AC_ARG_ENABLE([bitmap-later],
    [AS_HELP_STRING([--enable-bitmap-later],
        [enable fast startup of file server by not reading bitmap till
         needed])],
    [AS_IF([test x"$withval" = xyes],
        [AC_MSG_WARN([bitmap-later is only used by non-demand-attach
            fileservers.  Please migrate to demand-attach instead.])])],
    [enable_bitmap_later="no"])
AC_ARG_ENABLE([unix-sockets],
    [AS_HELP_STRING([--disable-unix-sockets],
        [disable use of unix domain sockets for fssync (defaults to enabled)])],
    [],
    [enable_unix_sockets="yes"])
AC_ARG_ENABLE([tivoli-tsm],
    [AS_HELP_STRING([--enable-tivoli-tsm],
        [enable use of the Tivoli TSM API libraries for butc support])],
    [],
    [enable_tivoli_tsm="no"])
AC_ARG_ENABLE([pthreaded-ubik],
    [AS_HELP_STRING([--disable-pthreaded-ubik],
        [disable installation of pthreaded ubik applications (defaults to
         enabled)])],
    [],
    [enable_pthreaded_ubik="yes"])
AC_ARG_ENABLE([ubik-read-while-write],
    [AS_HELP_STRING([--enable-ubik-read-while-write],
        [enable vlserver read from db cache during write locks (EXPERIMENTAL)])],
    [],
    [enable_ubik_read_while_write="no"])

dnl Kernel module build options.
AC_ARG_WITH([linux-kernel-headers],
    [AS_HELP_STRING([--with-linux-kernel-headers=path],
        [use the kernel headers found at path (optional, defaults to
         /lib/modules/`uname -r`/build, then /lib/modules/`uname -r`/source,
         and lastly /usr/src/linux)])
])
AC_ARG_WITH([linux-kernel-build],
    [AS_HELP_STRING([--with-linux-kernel-build=path],
        [use the kernel build found at path(optional, defaults to
        kernel headers path)]
)])
AC_ARG_WITH([bsd-kernel-headers],
    [AS_HELP_STRING([--with-bsd-kernel-headers=path],
        [use the kernel headers found at path (optional, defaults to
         /usr/src/sys)])
])
AC_ARG_WITH([bsd-kernel-build],
    [AS_HELP_STRING([--with-bsd-kernel-build=path],
        [use the kernel build found at path (optional, defaults to
         KSRC/i386/compile/GENERIC)])
])
AC_ARG_WITH([linux-kernel-packaging],
    [AS_HELP_STRING([--with-linux-kernel-packaging],
        [use standard naming conventions to aid Linux kernel build packaging
         (disables MPS, sets the kernel module name to openafs.ko, and
         installs kernel modules into the standard Linux location)])],
    [AC_SUBST([LINUX_KERNEL_PACKAGING], [yes])
     AC_SUBST([LINUX_LIBAFS_NAME], [openafs])],
    [AC_SUBST([LINUX_LIBAFS_NAME], [libafs])
])
AC_ARG_ENABLE([kernel-module],
    [AS_HELP_STRING([--disable-kernel-module],
        [disable compilation of the kernel module (defaults to enabled)])],
    [],
    [enable_kernel_module="yes"])
AC_ARG_ENABLE([redhat-buildsys],
    [AS_HELP_STRING([--enable-redhat-buildsys],
        [enable compilation of the redhat build system kernel (defaults to
         disabled)])],
    [],
    [enable_redhat_buildsys="no"])

dnl Installation locations.
AC_ARG_ENABLE([transarc-paths],
    [AS_HELP_STRING([--enable-transarc-paths],
        [use Transarc style paths like /usr/afs and /usr/vice])],
    [],
    [enable_transarc_paths="no"])

dnl Deprecated crypto
AC_ARG_ENABLE([kauth],
    [AS_HELP_STRING([--enable-kauth],
        [install the deprecated kauth server, pam modules, and utilities
         (defaults to disabled)])],
    [enable_pam="yes"],
    [enable_kauth="no"
     enable_pam="no"])

dnl Optimization and debugging flags.
AC_ARG_ENABLE([strip-binaries],
    [AS_HELP_STRING([--disable-strip-binaries],
        [disable stripping of symbol information from binaries (defaults to
         enabled)])],
    [],
    [enable_strip_binaries="maybe"])
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
        [enable compilation of the user space code with debugging information
         (defaults to disabled)])],
    [],
    [enable_debug="no"])
AC_ARG_ENABLE([optimize],
    [AS_HELP_STRING([--disable-optimize],
        [disable optimization for compilation of the user space code (defaults
         to enabled)])],
    [],
    [enable_optimize="yes"])
AC_ARG_ENABLE([warnings],
    [AS_HELP_STRING([--enable-warnings],
        [enable compilation warnings when building with gcc (defaults to
         disabled)])],
    [],
    [enable_warnings="no"])
AC_ARG_ENABLE([checking],
    [AS_HELP_STRING([--enable-checking],
        [turn compilation warnings into errors when building with gcc (defaults
         to disabled)])],
    [enable_checking="$enableval"],
    [enable_checking="no"])
AC_ARG_ENABLE([debug-locks],
    [AS_HELP_STRING([--enable-debug-locks],
        [turn on lock debugging assertions (defaults to disabled)])],
    [enable_debug_locks="$enableval"],
    [enable_debug_locks="no"])
AC_ARG_ENABLE([debug-kernel],
    [AS_HELP_STRING([--enable-debug-kernel],
        [enable compilation of the kernel module with debugging information
         (defaults to disabled)])],
    [],
    [enable_debug_kernel="no"])
AC_ARG_ENABLE([optimize-kernel],
    [AS_HELP_STRING([--disable-optimize-kernel],
        [disable compilation of the kernel module with optimization (defaults
         based on platform)])],
    [],
    [enable_optimize_kernel=""])
AC_ARG_ENABLE([debug-lwp],
    [AS_HELP_STRING([--enable-debug-lwp],
        [enable compilation of the LWP code with debugging information
         (defaults to disabled)])],
    [],
    [enable_debug_lwp="no"])
AC_ARG_ENABLE([optimize-lwp],
    [AS_HELP_STRING([--disable-optimize-lwp],
        [disable optimization for compilation of the LWP code (defaults to
         enabled)])],
    [],
    [enable_optimize_lwp="yes"])
AC_ARG_ENABLE([debug-pam],
    [AS_HELP_STRING([--enable-debug-pam],
        [enable compilation of the PAM code with debugging information
         (defaults to disabled)])],
    [],
    [enable_debug_pam="no"])
AC_ARG_ENABLE([optimize-pam],
    [AS_HELP_STRING([--disable-optimize-pam],
        [disable optimization for compilation of the PAM code (defaults to
         enabled)])],
    [],
    [enable_optimize_pam="yes"])
AC_ARG_ENABLE([linux-syscall-probing],
    [AS_HELP_STRING([--enable-linux-syscall-probing],
        [enable Linux syscall probing (defaults to autodetect)])],
    [],
    [enable_linux_syscall_probing="maybe"])
AC_ARG_ENABLE([linux-d_splice_alias-extra-iput],
    [AS_HELP_STRING([--enable-linux-d_splice_alias-extra-iput],
        [Linux kernels in the 3.17 series prior to 3.17.3 had a bug
         wherein error returns from the d_splice_alias() function were
         leaking a reference on the inode.  The bug was fixed for the
         3.17.3 kernel, and the possibility of an error return was only
         introduced in kernel 3.17, so only the narrow range of kernels
         is affected.  Enable this option for builds on systems with
         kernels affected by this bug, to manually release the reference
         on error returns and correct the reference counting.
         Linux commit 51486b900ee92856b977eacfc5bfbe6565028070 (or
         equivalent) is the fix for the upstream bug, so if such a commit
         is present, leave this option disabled.  We apologize
         that you are required to know this about your running kernel,
         but luckily only a narrow range of versions is affected.])],
    [],
    [enable_linux_d_splice_alias_extra_iput="no"])
AC_ARG_WITH([crosstools-dir],
    [AS_HELP_STRING([--with-crosstools-dir=path],
        [use path for native versions of rxgen, compile_et and config])
])

AC_ARG_WITH([xslt-processor],
        AS_HELP_STRING([--with-xslt-processor=ARG],
        [which XSLT processor to use (possible choices are: libxslt, saxon, xalan-j, xsltproc)]),
        [XSLTPROC="$withval"],
        [AC_CHECK_PROGS([XSLTPROC], [libxslt saxon xalan-j xsltproc], [echo])])

AC_ARG_WITH([html-xsl],
        AS_HELP_STRING([--with-html-xsl],
        [build HTML documentation using this stylesheet (default is html/chunk.dsl; specify either html/chunk.xsl or html/docbook.xsl)]),
        [HTML_XSL="$withval"],
        [HTML_XSL="html/chunk.xsl"])

AC_ARG_WITH([docbook2pdf],
        AS_HELP_STRING([--with-docbook2pdf=ARG],
        [which Docbook to PDF utility to use (possible choices are: fop, dblatex, docbook2pdf)]),
        [DOCBOOK2PDF="$withval"],
        [AC_CHECK_PROGS([DOCBOOK2PDF], [fop dblatex docbook2pdf], [echo])])

AC_ARG_WITH([docbook-stylesheets],
        AS_HELP_STRING([--with-docbook-stylesheets=ARG],
        [location of DocBook stylesheets (default is to search a set of likely paths)]),
        [DOCBOOK_STYLESHEETS="$withval"],
        [OPENAFS_SEARCH_DIRLIST([DOCBOOK_STYLESHEETS],
                [/usr/share/xml/docbook/stylesheet/nwalsh/current \
                 /usr/share/xml/docbook/stylesheet/nwalsh \
                 /usr/share/xml/docbook/xsl-stylesheets \
                 /usr/share/sgml/docbook/docbook-xsl-stylesheets \
                 /usr/share/sgml/docbook/xsl-stylesheets \
                 /usr/share/docbook-xsl \
                 /usr/share/sgml/docbkxsl \
                 /usr/local/share/xsl/docbook \
                 /sw/share/xml/xsl/docbook-xsl \
                 /opt/local/share/xsl/docbook-xsl],
                [$HTML_XSL])
           AS_IF([test "x$DOCBOOK_STYLESHEETS" = "x"],
                [AC_WARN([Docbook stylesheets not found; some documentation can't be built])
           ])
        ])

AC_ARG_WITH([dot],
        AS_HELP_STRING([--with-dot@<:@=PATH@:>@],
        [use graphviz dot to generate dependency graphs with doxygen (defaults to autodetect)]),
        [], [with_dot="maybe"])

enable_login="no"

])

AC_DEFUN([OPENAFS_OPTION_TESTS],[
# Fast restart
if test "$enable_supergroups" = "yes"; then
        AC_DEFINE(SUPERGROUPS, 1, [define if you want to have support for nested pts groups])
fi

if test "$enable_bitmap_later" = "yes"; then
        AC_DEFINE(BITMAP_LATER, 1, [define if you want to salvager to check bitmasks later])
fi

if test "$enable_unix_sockets" = "yes"; then
        AC_DEFINE(USE_UNIX_SOCKETS, 1, [define if you want to use UNIX sockets for fssync.])
        USE_UNIX_SOCKETS="yes"
else
        USE_UNIX_SOCKETS="no"
fi
AC_SUBST(USE_UNIX_SOCKETS)

if test "$enable_ubik_read_while_write" = "yes"; then
        AC_DEFINE(UBIK_READ_WHILE_WRITE, 1, [define if you want to enable ubik read while write])
fi

if test "$enable_namei_fileserver" = "yes"; then
        AC_DEFINE(AFS_NAMEI_ENV, 1, [define if you want to want namei fileserver])
        VFSCK=""
else
        if test "$enable_namei_fileserver" = "default"; then
                case $host in
                        *-solaris2.10*)
                                AC_MSG_WARN(Some Solaris 10 versions are not safe with the inode fileserver. Forcing namei. Override with --disable-namei-fileserver)
                                AC_DEFINE(AFS_NAMEI_ENV, 1, [define if you want to want namei fileserver])
                                VFSCK=""
                                ;;
                        *-solaris2.11*)
                                AC_MSG_WARN(Solaris 11 versions are not safe with the inode fileserver. Forcing namei. Override with --disable-namei-fileserver)
                                AC_DEFINE(AFS_NAMEI_ENV, 1, [define if you want to want namei fileserver])
                                VFSCK=""
                                ;;
                        *)
                                VFSCK="vfsck"
                                ;;
                esac
        else
                VFSCK="vfsck"
        fi
fi
])

AC_DEFUN([OPENAFS_MORE_OPTION_TESTS],[
if test "$enable_debug_locks" = yes; then
        AC_DEFINE(OPR_DEBUG_LOCKS, 1, [turn on lock debugging in opr])
fi

if test "$ac_cv_header_security_pam_modules_h" = yes -a "$enable_pam" = yes; then
        HAVE_PAM="yes"
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

if test "$enable_kauth" = yes; then
        INSTALL_KAUTH="yes"
else
        INSTALL_KAUTH="no"
fi
AC_SUBST(INSTALL_KAUTH)
])

AC_DEFUN([OPENAFS_YET_MORE_OPTION_TESTS],[
if test "x$enable_kernel_module" = "xyes"; then
ENABLE_KERNEL_MODULE=libafs
fi

if test "x$enable_pthreaded_ubik" = "xyes"; then
ENABLE_PTHREADED_UBIK=yes
fi
])
