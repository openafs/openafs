
AC_DEFUN([OPENAFS_OPTIONS],[

dnl System identity.
AC_ARG_WITH([afs-sysname],
    [AS_HELP_STRING([--with-afs-sysname=sys], [use sys for the afs sysname])])

dnl General feature options.
AC_ARG_ENABLE([pam],
    [AS_HELP_STRING([--disable-pam], [disable PAM support])],
    ,
    [enable_pam="yes"])
AC_ARG_ENABLE([namei-fileserver],
    [AS_HELP_STRING([--enable-namei-fileserver],
        [force compilation of namei fileserver in preference to inode
         fileserver])],
    , 
    [enable_namei_fileserver="default"])
AC_ARG_ENABLE([supergroups],
    [AS_HELP_STRING([--enable-supergroups],
        [enable support for nested pts groups])],
    , 
    [enable_supergroups="no"])
AC_ARG_ENABLE([bitmap-later],
    [AS_HELP_STRING([--enable-bitmap-later],
        [enable fast startup of file server by not reading bitmap till
         needed])],
    , 
    [enable_bitmap_later="no"])
AC_ARG_ENABLE([unix-sockets],
    [AS_HELP_STRING([--disable-unix-sockets],
        [disable use of unix domain sockets for fssync (defaults to enabled)])],
    ,
    [enable_unix_sockets="yes"])
AC_ARG_ENABLE([tivoli-tsm],
    [AS_HELP_STRING([--enable-tivoli-tsm],
        [enable use of the Tivoli TSM API libraries for butc support])],
    , 
    [enable_tivoli_tsm="no"])
AC_ARG_ENABLE([pthreaded-ubik],
    [AS_HELP_STRING([--enable-pthreaded-ubik],
        [enable installation of pthreaded ubik applications (defaults to
         disabled)])],
    ,
    [enable_pthreaded_ubik="no"])

dnl Kernel module build options.
AC_ARG_WITH([linux-kernel-headers],
    [AS_HELP_STRING([--with-linux-kernel-headers=path],
        [use the kernel headers found at path (optional, defaults to
         /lib/modules/`uname -r`/build, then /lib/modules/`uname -r`/source,
         then /usr/src/linux-2.4, and lastly /usr/src/linux)])])
AC_ARG_WITH([linux-kernel-build],
    [AS_HELP_STRING([--with-linux-kernel-build=path],
	[use the kernel build found at path(optional, defaults to 
	kernel headers path)])])
AC_ARG_WITH([bsd-kernel-headers],
    [AS_HELP_STRING([--with-bsd-kernel-headers=path],
        [use the kernel headers found at path (optional, defaults to
         /usr/src/sys)])])
AC_ARG_WITH([bsd-kernel-build],
    [AS_HELP_STRING([--with-bsd-kernel-build=path], 
        [use the kernel build found at path (optional, defaults to
         KSRC/i386/compile/GENERIC)])])
AC_ARG_WITH([linux-kernel-packaging],
    [AS_HELP_STRING([--with-linux-kernel-packaging],
        [use standard naming conventions to aid Linux kernel build packaging
         (disables MPS, sets the kernel module name to openafs.ko, and
         installs kernel modules into the standard Linux location)])],
    [AC_SUBST(LINUX_KERNEL_PACKAGING, "yes")
     AC_SUBST(LINUX_LIBAFS_NAME, "openafs")],
    [AC_SUBST(LINUX_LIBAFS_NAME, "libafs")])
AC_ARG_ENABLE([kernel-module],
    [AS_HELP_STRING([--disable-kernel-module],
        [disable compilation of the kernel module (defaults to enabled)])],
    , 
    [enable_kernel_module="yes"])
AC_ARG_ENABLE([redhat-buildsys],
    [AS_HELP_STRING([--enable-redhat-buildsys],
        [enable compilation of the redhat build system kernel (defaults to
         disabled)])],
    ,
    [enable_redhat_buildsys="no"])

dnl Installation locations.
AC_ARG_ENABLE([transarc-paths],
    [AS_HELP_STRING([--enable-transarc-paths],
        [use Transarc style paths like /usr/afs and /usr/vice])],
    , 
    [enable_transarc_paths="no"])

dnl Optimization and debugging flags.
AC_ARG_ENABLE([strip-binaries],
    [AS_HELP_STRING([--disable-strip-binaries],
        [disable stripping of symbol information from binaries (defaults to
         enabled)])],
    ,
    [enable_strip_binaries="maybe"])
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
        [enable compilation of the user space code with debugging information
         (defaults to disabled)])],
    , 
    [enable_debug="no"])
AC_ARG_ENABLE([optimize],
    [AS_HELP_STRING([--disable-optimize],
        [disable optimization for compilation of the user space code (defaults
         to enabled)])],
    , 
    [enable_optimize="yes"])
AC_ARG_ENABLE([warnings],
    [AS_HELP_STRING([--enable-warnings],
        [enable compilation warnings when building with gcc (defaults to
         disabled)])],
    ,
    [enable_warnings="no"])
AC_ARG_ENABLE([checking],
    [AS_HELP_STRING([--enable-checking],
	[turn compilation warnings into errors when building with gcc (defaults
	 to disabled)])],
    [enable_checking="$enableval"],
    [enable_checking="no"])
AC_ARG_ENABLE([debug-kernel],
    [AS_HELP_STRING([--enable-debug-kernel],
        [enable compilation of the kernel module with debugging information
         (defaults to disabled)])],
    ,
    [enable_debug_kernel="no"])
AC_ARG_ENABLE([optimize-kernel],
    [AS_HELP_STRING([--disable-optimize-kernel],
        [disable compilation of the kernel module with optimization (defaults
         based on platform)])],
    , 
    [enable_optimize_kernel=""])
AC_ARG_ENABLE([debug-lwp],
    [AS_HELP_STRING([--enable-debug-lwp],
        [enable compilation of the LWP code with debugging information
         (defaults to disabled)])],
    ,
    [enable_debug_lwp="no"])
AC_ARG_ENABLE([optimize-lwp],
    [AS_HELP_STRING([--disable-optimize-lwp],
        [disable optimization for compilation of the LWP code (defaults to
         enabled)])],
    ,
    [enable_optimize_lwp="yes"])
AC_ARG_ENABLE([debug-pam],
    [AS_HELP_STRING([--enable-debug-pam],
        [enable compilation of the PAM code with debugging information
         (defaults to disabled)])],
    ,
    [enable_debug_pam="no"])
AC_ARG_ENABLE([optimize-pam],
    [AS_HELP_STRING([--disable-optimize-pam],
        [disable optimization for compilation of the PAM code (defaults to
         enabled)])],
    ,
    [enable_optimize_pam="yes"])
AC_ARG_ENABLE([linux-syscall-probing],
    [AS_HELP_STRING([--enable-linux-syscall-probing],
	[enable Linux syscall probing (defaults to autodetect)])],
    ,
    [enable_linux_syscall_probing="maybe"])
    
AC_ARG_ENABLE([linux-d_splice_alias-extra-iput],
    [AS_HELP_STRING([--enable-linux-d_splice_alias-extra-iput],
	[Linux has introduced an incompatible behavior change in the
	 d_splice_alias function with no reliable way to determine which
	 behavior will be produced.  If Linux commit
	 51486b900ee92856b977eacfc5bfbe6565028070 (or equivalent) has been
	 applied to your kernel, disable this option.  If that commit is
	 not present in your kernel, enable this option.  We apologize
	 that you are required to know this about your running kernel.])],
    [],
    [case $system in
    *-linux*)
	AS_IF([test "x$LOGNAME" != "xbuildslave" &&
	    test "x$LOGNAME" != "xbuildbot"],
	    [AC_ERROR([Linux users must specify either
		--enable-linux-d_splice_alias-extra-iput or
		--disable-linux-d_splice_alias-extra-iput])],
	    [enable_linux_d_splice_alias_extra_iput="no"])
     esac
    ])
AC_ARG_WITH([xslt-processor],
	AS_HELP_STRING([--with-xslt-processor=ARG],
	[which XSLT processor to use (possible choices are: libxslt, saxon, xalan-j, xsltproc)]),
       	XSLTPROC="$withval",
	AC_CHECK_PROGS([XSLTPROC], [libxslt saxon xalan-j xsltproc], [echo]))

AC_ARG_WITH([html-xsl], 
        AS_HELP_STRING([--with-html-xsl],
	[build HTML documentation using Norman Walsh's DocBook XSL stylesheets (default is no; specify a path to chunk.xsl or docbook.xsl)]),
	HTML_XSL="$withval",
	HTML_XSL=no)

AC_ARG_WITH([docbook2pdf],
	AS_HELP_STRING([--with-docbook2pdf=ARG],
	[which Docbook to PDF utility to use (possible choices are: docbook2pdf, dblatex)]),
       	DOCBOOK2PDF="$withval",
	AC_CHECK_PROGS([DOCBOOK2PDF], [docbook2pdf dblatex], [echo]))

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

dnl if test "$ac_cv_setsockopt_iprecverr" = "yes"; then
dnl 	AC_DEFINE(ADAPT_PMTU, 1, [define if you want to decode icmp unreachable packets to discover path mtu])
dnl fi

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
])


AC_DEFUN([OPENAFS_YET_MORE_OPTION_TESTS],[
if test "x$enable_kernel_module" = "xyes"; then
ENABLE_KERNEL_MODULE=libafs
fi

if test "x$enable_pthreaded_ubik" = "xyes"; then
ENABLE_PTHREADED_UBIK=yes
fi
])
