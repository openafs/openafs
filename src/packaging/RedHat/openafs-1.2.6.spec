%define afsvers 1.2.6
%define pkgrel 1

# Define your particular Red Hat and kernel versions:
#	For Linux 2.2:  22
#	For Linux 2.4:	24
#
%define osvers rh7.1
%define kernvers 24

# This is where to look for kernel-build includes files.
# Most likely you don't want to change this, but
# depending on your situation you may want:
# 	Linux 2.2:
#		kbase = /usr/src/linux-
#		kend = ""
#	Linux 2.4:
#		kbase = /lib/modules/
#		kend = /build
#
%define kbase /usr/src/linux-
%define kend ""

# Set 'debugspec' to 1 if you want to debug the spec file.  This will
# not remove the installed tree as part of the %clean operation
%define debugspec 0

# Set 'enterprisekernelsupport' to 1 if you want to build the
# kernel module for the enterprise kernel
# Note: This will only work for kernvers == 24 on i686
%define enterprisekernelsupport 1

# Set 'bigmemkernelsupport' to 1 if you want to build the
# kernel module for the bigmem kernel
# Note: This will only work for kernvers == 24 on i686
%define bigmemkernelsupport 1

# Set 'krb5support' to 1 if you want to build the openafs-krb5 package
# to distribute aklog and asetkey
%define krb5support 1

# OpenAFS configuration options
%define enable_bitmap_later 0
%define enable_bos_restricted_mode 0
%define enable_fast_restart 0

#######################################################################
# You probably don't need to change anything beyond this line
# NOTE: If you do, please email me!!!

Summary: OpenAFS distributed filesystem
Name: openafs
Version: %{afsvers}
Release: %{osvers}.%{pkgrel}
Copyright: IPL
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Packager: Derek Atkins <warlord@MIT.EDU>
Group: Networking/Filesystems
BuildRequires: kernel-source
%if "%{osvers}" != "rh6.2"
# Newer versions of Red Hat require pam-devel in order to build
BuildRequires: pam-devel
%endif

Source0: http://www.openafs.org/dl/openafs/${afsvers}/openafs-%{afsvers}-src.tar.gz
Source1: http://www.openafs.org/dl/openafs/${afsvers}/openafs-%{afsvers}-doc.tar.gz
Source2: openafs-ThisCell
# http://grand.central.org/dl/cellservdb/CellServDB
Source3: openafs-CellServDB
Source4: openafs-SuidCells
Source5: openafs-cacheinfo
Source6: openafs-afsmodname
Source7: openafs-LICENSE.Sun
Source8: openafs-README
Source10: http://www.openafs.org/dl/openafs/${afsvers}/RELNOTES-%{afsvers}
Source11: http://www.openafs.org/dl/openafs/${afsvers}/ChangeLog

Source20: openafs-krb5-1.3.tar.gz

Patch0: openafs-%{afsvers}-rc.patch

Patch20: openafs-krb5-1.3-1.2.1.diff.gz
Patch21: openafs-krb5-1.3-configure.patch

%description
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides common files shared across all the various
OpenAFS packages but are not necessarily tied to a client or server.

%package client
Requires: binutils, openafs-kernel, openafs = %{PACKAGE_VERSION}
Summary: OpenAFS Filesystem Client
Group: Networking/Filesystem

%description client
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides basic client support to mount and manipulate
AFS.

%package server
Requires: openafs-kernel, openafs = %{PACKAGE_VERSION}
Summary: OpenAFS Filesystem Server
Group: Networking/Filesystems

%description server
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides basic server support to host files in an AFS
Cell.

%package devel
Summary: OpenAFS Development Libraries and Headers
Group: Development/Filesystems

%description devel
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides static development libraries and headers needed
to compile AFS applications.  Note: AFS currently does not provide
shared libraries.

%package kernel
Summary: OpenAFS Kernel Module(s)
Requires: openafs = %{PACKAGE_VERSION}
Group: Networking/Filesystems

%description kernel
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides precompiled AFS kernel modules for various
kernels.

%package kernel-source
Summary: OpenAFS Kernel Module source tree
Group: Networking/Filesystems

%description kernel-source
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides the source code to build your own AFS kernel
module.

%package compat
Summary: OpenAFS client compatibility symlinks
Requires: openafs = %{PACKAGE_VERSION}, openafs-client = %{PACKAGE_VERSION}
Group: Networking/Filesystems
Obsoletes: openafs-client-compat

%description compat
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides compatibility symlinks in /usr/afsws.  It is
completely optional, and is only necessary to support legacy
applications and scripts that hard-code the location of AFS client
programs.

%package kpasswd
Summary: OpenAFS KA kpasswd support
Requires: openafs
Group: Networking/Filesystems

%description kpasswd
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides the compatibility symlink for kpasswd, in case
you are using KAserver instead of Krb5.

%if %{krb5support}
%package krb5
Summary: OpenAFS programs to use with krb5
Requires: openafs = %{PACKAGE_VERSION}
Group: Networking/Filesystems
BuildRequires: krb5-devel

%description krb5
The AFS distributed filesystem.  AFS is a distributed filesystem
allowing cross-platform sharing of files among multiple computers.
Facilities are provided for access control, authentication, backup and
administrative management.

This package provides compatibility programs so you can use krb5
to authenticate to AFS services, instead of using AFS's homegrown
krb4 lookalike services.
%endif

#
# PREP
#

%prep
%setup -q -b 1
%setup -q -T -D -a 20

%patch0 -p0
%patch20 -p0
%patch21 -p0

###
### build
###
%build

%ifarch i386 i486 i586 i686 athlon
sysbase=i386
sysname=${sysbase}_linux%{kernvers}
%else
sysbase=%{_arch}
sysname=${sysbase}_linux%{kernvers}
%endif

if [ %{kernvers} = 22 ]; then
   kv='2\.2\.'
elif [ %{kernvers} = 24 ]; then
   kv='2\.4\.'
else
   echo "I don't know how to build $sysname"
   exit 1
fi

%ifarch i386 i486 i586 i686 athlon
archlist="i386 i586 i686 athlon"
%else
archlist=${sysbase}
%endif

#
# PrintDefine var value statements file
#
PrintDefine() {
    case $3 in
    *ifn*)
	echo "#ifndef $1" >> $4
	;;
    esac
    case $3 in
    *und*)
	echo "#undef $1" >> $4
	;;
    esac
    case $3 in
    *def*)
	echo "#define $1 $2" >> $4
	;;
    esac
    case $3 in
    *end*)
	echo "#endif" >> $4
	;;
    esac
    case $3 in
    *inc*)
	echo "#include $1" >> $4
	;;
    esac


    case $3 in
    *nl*)
	echo "" >> $4
	;;
    esac
}

# PrintRedhatKernelFix arch mp file
PrintRedhatKernelFix() {
    arch="$1"
    up=0
    smp=0
    ent=0
    bm=0
    if [ "$2" = "MP" ]; then
	smp=1
    elif [ "$2" = "EP" ]; then
	ent=1
    elif [ "$2" = "BM" ]; then
	bm=1
    else
	up=1
    fi
    file="$3"

    # deal with the various boot kernels
    boot=0
    bootsmp=0

    # arch of 'BOOT' == 386
    if [ "$arch" = "BOOT" ]; then
	if [ "$up" = 1 ]; then
	    boot=1
	    up=0
	elif [ "$smp" = 1 ]; then
	    bootsmp=1
	    smp=0
	fi
	arch=i386
    fi

    rm -f $file
    touch $file

    PrintDefine "REDHAT_FIX_H" "" ifn,def,nl $file

    PrintDefine "__BOOT_KERNEL_ENTERPRISE" $ent und,def,nl $file
    PrintDefine "__BOOT_KERNEL_BIGMEM" $bm und,def,nl $file
    PrintDefine "__BOOT_KERNEL_SMP" $smp und,def,nl $file
    PrintDefine "__BOOT_KERNEL_UP" $up und,def,nl $file
    PrintDefine "__BOOT_KERNEL_BOOT" $boot und,def,nl $file
    PrintDefine "__BOOT_KERNEL_BOOTSMP" $bootsmp und,def,nl $file

    PrintDefine \"/boot/kernel.h\" "" inc,nl $file	# include file

    for ar in $archlist ; do
	if [ "$ar" = "$arch" ]; then
	    PrintDefine "__MODULE_KERNEL_$ar" "1" ifn,def,end $file
	else
	    PrintDefine "__MODULE_KERNEL_$ar" "" und $file	# undef
        fi
    done
    echo "" >> $file

    PrintDefine "" "" end $file

    if [ %{debugspec} = 1 ] ; then
	echo "Kernel Configuration File for Red Hat kernels:"
	cat $file
    fi
}

# Pick up all the 'appropriate' kernels
kvers=`ls -d %{kbase}* | sed 's^%{kbase}^^g' | grep $kv`

# Choose the last one for now.. It doesn't really matter, really.
hdrdir=`ls -d %{kbase}*%{kend} | grep $kv | tail -1`

config_opts="--enable-redhat-buildsys \
%if %{enable_bitmap_later}
	--enable-bitmap-later \
%endif
%if %{enable_bos_restricted_mode}
	--enable-bos-restricted-mode \
%endif
%if %{enable_fast_restart}
	--enable-fast-restart \
%endif
	--enable-transarc-paths"

# Configure AFS
./configure --with-afs-sysname=${sysname} \
	--with-linux-kernel-headers=$hdrdir $config_opts

# Build the user-space AFS stuff
make dest_nolibafs

# Build the libafs tree
make only_libafs_tree

# Now build all the kernel modules
for vers in $kvers ; do

  # Reconfigure sources for this kernel version, to catch various
  # kernel params in the configure script.  Yes. this takes more time,
  # but it's worth it in the long run..  But first remove config.cache
  # to be sure we get a clean configuration.
  rm -f config.cache
  ./configure --with-afs-sysname=${sysname} \
	--with-linux-kernel-headers=%{kbase}$vers%{kend} \
	$config_opts

  KTL="SP MP"
%if %{enterprisekernelsupport}
  # See if we should build EP support
  if grep -q -r __BOOT_KERNEL_ENTERPRISE %{kbase}$vers%{kend}/include
  then
    KTL="${KTL} EP"
  fi
%endif
%if %{bigmemkernelsupport}
  # See if we should build BM support
  if grep -q -r __BOOT_KERNEL_BIGMEM %{kbase}$vers%{kend}/include
  then
    KTL="${KTL} BM"
  fi
%endif
 
  for mp in $KTL; do
    # ... for all appropriate 'architectures'...
    if [ %{kernvers} = 22 ]; then
	# For 2.2 kernels, just do MP and SP kernels; force EP into i686

        arch=${sysbase}
        if [ $mp = EP -a ${sysbase} = i386 ]; then
	    arch=i686
	fi

	PrintRedhatKernelFix $arch $mp src/config/redhat-fix.h
	make dest_only_libafs LOCAL_SMP_DEF=-DREDHAT_FIX MPS=$mp

    elif [ %{kernvers} = 24 ]; then
	# For 2.4 kernels, need to build modules for each architecture!

	for arch in $archlist ; do

	    # build SP and MP on all architectures.
	    # build EP and BM only on i686
            if [ $mp = SP -o $mp = MP -o \
		 \( $mp = EP -a $arch = i686 \) -o \
		 \( $mp = BM -a $arch = i686 \) ]; then
		PrintRedhatKernelFix $arch $mp src/config/redhat-fix.h
		make dest_only_libafs LOCAL_SMP_DEF=-DREDHAT_FIX \
		    LINUX_MODULE_NAME="-$arch" MPS=$mp
	    fi
	done

    else	    
	echo "I don't know how to build $sysname"
	exit 1
    fi
  done
done

rm -f src/config/redhat-fix.h

%if %{krb5support}
# Now build aklog/asetkey
(cd openafs-krb5-1.3/src &&
	autoconf &&
	./configure --prefix=/usr --with-krb5=/usr/kerberos \
		--with-afs=`pwd`/../../${sysname}/dest/ && \
	make all && \
	make install DESTDIR=`pwd`/../../${sysname}/dest/ INSTALL_BIN=/bin \
		INSTALL_SBIN=/etc)
%endif

###
### install
###
%install
[ $RPM_BUILD_ROOT != / ] && rm -rf $RPM_BUILD_ROOT

%ifarch i386 i486 i586 i686 athlon
sysbase=i386
sysname=${sysbase}_linux%{kernvers}
%else
sysbase=%{_arch}
sysname=${sysbase}_linux%{kernvers}
%endif

# Build install tree
mkdir -p $RPM_BUILD_ROOT/usr/sbin
mkdir -p $RPM_BUILD_ROOT/etc/sysconfig
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
mkdir -p $RPM_BUILD_ROOT/etc/openafs
mkdir -p $RPM_BUILD_ROOT/lib/security
mkdir -p $RPM_BUILD_ROOT/usr/afs/logs
mkdir -p $RPM_BUILD_ROOT/usr/vice/etc
mkdir -p $RPM_BUILD_ROOT/usr/vice/cache
chmod 700 $RPM_BUILD_ROOT/usr/vice/cache

# Copy files from dest to the appropriate places in BuildRoot
tar cf - -C ${sysname}/dest bin include lib | tar xf - -C $RPM_BUILD_ROOT/usr
tar cf - -C ${sysname}/dest/etc . | tar xf - -C $RPM_BUILD_ROOT/usr/sbin
tar cf - -C ${sysname}/dest/root.server/usr/afs bin | tar xf - -C $RPM_BUILD_ROOT/usr/afs
tar cf - -C ${sysname}/dest/root.client/usr/vice/etc afsd modload | tar xf - -C $RPM_BUILD_ROOT/usr/vice/etc

# Link kpasswd to kapasswd
ln -f $RPM_BUILD_ROOT/usr/bin/kpasswd $RPM_BUILD_ROOT/usr/bin/kapasswd

# Copy root.client config files
install -m 755 ${sysname}/dest/root.client/usr/vice/etc/afs.conf $RPM_BUILD_ROOT/etc/sysconfig/afs
install -m 755 ${sysname}/dest/root.client/usr/vice/etc/afs.rc $RPM_BUILD_ROOT/etc/rc.d/init.d/afs

# Copy PAM modules
install -m 755 ${sysname}/dest/lib/pam* $RPM_BUILD_ROOT/lib/security

# PAM symlinks
ln -sf pam_afs.so.1 $RPM_BUILD_ROOT/lib/security/pam_afs.so
ln -sf pam_afs.krb.so.1 $RPM_BUILD_ROOT/lib/security/pam_afs.krb.so

# Populate /usr/vice/etc
uve=$RPM_BUILD_ROOT/usr/vice/etc
install -p -m 644 $RPM_SOURCE_DIR/openafs-CellServDB $uve/CellServDB
install -p -m 644 $RPM_SOURCE_DIR/openafs-SuidCells $uve/SuidCells
install -p -m 644 $RPM_SOURCE_DIR/openafs-ThisCell $uve/ThisCell
install -p -m 644 $RPM_SOURCE_DIR/openafs-cacheinfo $uve/cacheinfo
install -p -m 755 $RPM_SOURCE_DIR/openafs-afsmodname $uve/afsmodname

#
# Build the SymTable
symtable=$RPM_BUILD_ROOT/usr/vice/etc/modload/SymTable
rm -f $symtable
echo "# SymTable, automatically generated" > $symtable
echo "# symbol	version	cpu	module" >> $symtable
echo "" >> $symtable

$RPM_BUILD_ROOT/usr/vice/etc/afsmodname -x -f $symtable \
	$RPM_BUILD_ROOT/usr/vice/etc/modload/libafs*.o

#
# install kernel-source
#

# Install the kernel module source tree
mkdir -p $RPM_BUILD_ROOT/usr/src/openafs-kernel-%{afsvers}/src
tar cf - -C libafs_tree . | \
	tar xf - -C $RPM_BUILD_ROOT/usr/src/openafs-kernel-%{afsvers}/src

# Next, copy the LICENSE Files, README
install -m 644 src/LICENSE $RPM_BUILD_ROOT/usr/src/openafs-kernel-%{afsvers}/LICENSE.IBM
install -m 644 $RPM_SOURCE_DIR/openafs-LICENSE.Sun $RPM_BUILD_ROOT/usr/src/openafs-kernel-%{afsvers}/LICENSE.Sun
install -m 644 $RPM_SOURCE_DIR/openafs-README $RPM_BUILD_ROOT/usr/src/openafs-kernel-%{afsvers}/README

#
# Install DOCUMENTATION
#

# Build the DOC directory
mkdir -p $RPM_BUILD_ROOT/$RPM_DOC_DIR/openafs-%{afsvers}
tar cf - -C doc LICENSE html pdf | \
    tar xf - -C $RPM_BUILD_ROOT/$RPM_DOC_DIR/openafs-%{afsvers}
install -m 644 $RPM_SOURCE_DIR/RELNOTES-%{afsvers} $RPM_BUILD_ROOT/$RPM_DOC_DIR/openafs-%{afsvers}
install -m 644 $RPM_SOURCE_DIR/ChangeLog $RPM_BUILD_ROOT/$RPM_DOC_DIR/openafs-%{afsvers}

#
# create filelist
#
grep -v "^#" >openafs-file-list <<EOF-openafs-file-list
/usr/bin/afsmonitor
/usr/bin/bos
/usr/bin/fs
/usr/bin/kapasswd
/usr/bin/kpasswd
/usr/bin/klog
/usr/bin/klog.krb
/usr/bin/pagsh
/usr/bin/pagsh.krb
/usr/bin/pts
/usr/bin/scout
/usr/bin/sys
/usr/bin/tokens
/usr/bin/tokens.krb
/usr/bin/translate_et
/usr/bin/udebug
/usr/bin/unlog
/usr/sbin/backup
/usr/sbin/butc
/usr/sbin/fms
/usr/sbin/fstrace
/usr/sbin/kas
/usr/sbin/read_tape
/usr/sbin/restorevol
/usr/sbin/rxdebug
/usr/sbin/uss
/usr/sbin/vos
EOF-openafs-file-list

#
# Install compatiblity links
#
for d in bin:bin etc:sbin; do
  olddir=`echo $d | sed 's/:.*$//'`
  newdir=`echo $d | sed 's/^.*://'`
  mkdir -p $RPM_BUILD_ROOT/usr/afsws/$olddir
  for f in `cat openafs-file-list`; do
    if echo $f | grep -q /$newdir/; then
      fb=`basename $f`
      ln -sf /usr/$newdir/$fb $RPM_BUILD_ROOT/usr/afsws/$olddir/$fb
    fi
  done
done


###
### clean
###
%clean
rm -f openafs-file-list
[ "$RPM_BUILD_ROOT" != "/" -a "x%{debugspec}" != "x1" ] && \
	rm -fr $RPM_BUILD_ROOT


###
### scripts
###
%pre compat
if [ -e /usr/afsws ]; then
        /bin/rm -fr /usr/afsws
fi

%post
chkconfig --add afs

%post client
if [ ! -d /afs ]; then
	mkdir /afs
	chown root.root /afs
	chmod 0755 /afs
fi

echo
echo The AFS cache is configured for 100 MB. Edit the 
echo /usr/vice/etc/cacheinfo file to change this before
echo running AFS for the first time. You should also
echo set your home cell in /usr/vice/etc/ThisCell.
echo
echo Also, you may want to edit /etc/pam.d/login and 
echo possibly others there to get an AFS token on login.
echo Put the line:
echo 
echo    auth	   sufficient   /lib/security/pam_afs.so try_first_pass ignore_root
echo
echo before the one for pwdb.
echo

%post server
if [ -f /etc/sysconfig/afs ] ; then
	srv=`grep ^AFS_SERVER /etc/sysconfig/afs | sed 's/^AFS_SERVER[\s]*=[\s]*//'`
	if [ "x$srv" = "xon" ] ; then
		exit 0
	fi
fi

echo
echo Be sure to edit /etc/sysconfig/afs and turn AFS_SERVER on
echo

%preun
if [ $1 = 0 ] ; then
        /etc/rc.d/init.d/afs stop
        chkconfig --del afs
	[ -d /afs ] && rmdir /afs
fi

###
### file lists
###
%files -f openafs-file-list
%defattr(-,root,root)
%config /etc/sysconfig/afs
%doc %{_docdir}/openafs-%{afsvers}
/etc/rc.d/init.d/afs

%files client
%defattr(-,root,root)
%dir /usr/vice
%dir /usr/vice/cache
%dir /usr/vice/etc
%dir /usr/vice/etc/modload
%config /usr/vice/etc/CellServDB
%config /usr/vice/etc/SuidCells
%config /usr/vice/etc/ThisCell
%config /usr/vice/etc/cacheinfo
/usr/bin/cmdebug
/usr/bin/up
/usr/vice/etc/afsd
/usr/vice/etc/afsmodname
/lib/security/pam_afs.krb.so.1
/lib/security/pam_afs.krb.so
/lib/security/pam_afs.so.1
/lib/security/pam_afs.so

%files server
%defattr(-,root,root)
%dir /usr/afs
%dir /usr/afs/bin
%dir /usr/afs/logs
/usr/afs/bin/bosserver
/usr/afs/bin/buserver
/usr/afs/bin/fileserver
# Should we support KAServer?
/usr/afs/bin/kaserver
/usr/afs/bin/kpwvalid
/usr/afs/bin/pt_util
/usr/afs/bin/ptserver
/usr/afs/bin/salvager
/usr/afs/bin/upclient
/usr/afs/bin/upserver
/usr/afs/bin/vlserver
/usr/afs/bin/volinfo
/usr/afs/bin/volserver
/usr/sbin/prdb_check
/usr/sbin/vldb_check
/usr/sbin/vldb_convert

%files devel
%defattr(-,root,root)
/usr/bin/rxgen
/usr/include/afs
/usr/include/des.h
/usr/include/des_conf.h
/usr/include/des_odd.h
/usr/include/lock.h
/usr/include/lwp.h
/usr/include/mit-cpyright.h
/usr/include/potpourri.h
/usr/include/preempt.h
/usr/include/rx
/usr/include/timer.h
/usr/include/ubik.h
/usr/include/ubik_int.h
/usr/lib/afs
/usr/lib/libafsauthent.a
/usr/lib/libafsrpc.a
/usr/lib/libdes.a
/usr/lib/liblwp.a
/usr/lib/librx.a
/usr/lib/librxkad.a
/usr/lib/librxstat.a
/usr/lib/libubik.a

%files kernel
%defattr(-,root,root)
/usr/vice/etc/modload/libafs*.o
/usr/vice/etc/modload/SymTable

%files kernel-source
%defattr(-,root,root)
/usr/src/openafs-kernel-%{afsvers}/LICENSE.IBM
/usr/src/openafs-kernel-%{afsvers}/LICENSE.Sun
/usr/src/openafs-kernel-%{afsvers}/README
/usr/src/openafs-kernel-%{afsvers}/src

%files compat
%defattr(-,root,root)
/usr/afsws

%files kpasswd
%defattr(-,root,root)
/usr/bin/kpasswd
/usr/bin/kpwvalid

%if %{krb5support}
%files krb5
%defattr(-,root,root)
/usr/bin/aklog
/usr/sbin/asetkey
%endif
