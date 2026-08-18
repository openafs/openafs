#
# OpenAFS RPM spec file for RHEL family distributions
#
# This spec file builds the OpenAFS packages for RHEL, Fedora, and related
# distributions.
#
# The userspace packages and a kernel module for the running kernel are built
# by default. The following rpmbuild options set the package version and
# change what is built:
#
# --define "openafs_version <version>"
#       The OpenAFS version, as given by build-tools/git-version. This is
#       the version in the source tarball and kernel module source names.
#       The default is UNKNOWN. makesrpm.pl sets this in the spec file it
#       generates.
#
# --define "package_version <version>"
#       The RPM package version. The default is openafs_version.
#       makesrpm.pl sets this in the spec file it generates.
#
# --define "package_release <release>"
#       The RPM package release. The default is 1. makesrpm.pl sets this
#       in the spec file it generates.
#
# --define "kernel_version <version>"
#       The kernel version to build the kernel module against. The default
#       is the running kernel.
#
# --define "kernel_headers <path>"
#       The path to the kernel headers for kernel_version. The default is
#       /usr/src/kernels/<kernel_version>.
#
# --define "kernel_modules <path>"
#       The kernel module installation path for kernel_version. The default
#       is /usr/lib/modules/<kernel_version>.
#
# --define "build_userspace 0"
#       Do not build the userspace packages.
#
# --define "build_modules 0"
#       Do not build the kernel module package.
#
# --define "build_dkmspkg 0"
#       Do not build the dkms-openafs package.
#
# --define "source_date_epoch <seconds>"
#       The build timestamp, in seconds since the epoch. The default is the
#       current time.
#
# --without authlibs
#       Do not build the openafs-authlibs package.
#
# --without krb5
#       Build without Kerberos 5 support, and do not build the openafs-krb5
#       package (aklog, asetkey, and akeyconvert).
#
# --with supergroups
#       Enable support for supergroups.
#
# --with kauth
#       Build the openafs-kauth-client and openafs-kauth-server packages,
#       which contain the legacy kaserver and related programs.
#

#-----------------------------------------------------------------------------
# Macro definitions
#-----------------------------------------------------------------------------

#
# Package version
#

%if 0%{!?openafs_version:1}
%global openafs_version UNKNOWN
%endif

%if 0%{!?package_version:1}
%global package_version %{openafs_version}
%endif

%if 0%{!?package_release:1}
%global package_release 1
%endif

%global dkms_version %{package_version}-%{package_release}%{?dist}

#
# Build options
#

%if 0%{!?build_userspace:1}
%global build_userspace 1
%endif

%if 0%{!?build_modules:1}
%global build_modules 1
%endif

%if 0%{!?build_dkmspkg:1}
%global build_dkmspkg 1
%endif

%bcond_without authlibs
%bcond_without krb5
%bcond_with kauth
%bcond_with supergroups

#
# Supported distributions
#

# Minimum supported RHEL release.
%if 0%{?rhel}
%if 0%{?rhel} < 8
# Stop the build with an invalid tag.
unsupported_rhel_release: %{rhel}
%endif
%endif

# Minimum supported Fedora release.
%if 0%{?fedora}
%if 0%{?fedora} < 30
# Stop the build with an invalid tag.
unsupported_fedora_release: %{fedora}
%endif
%endif

# Minimum supported Amazon Linux release.
%if 0%{?amzn}
%if 0%{?amzn} < 2023
# Stop the build with an invalid tag.
unsupported_amzn_release: %{amzn}
%endif
%endif

#
# Kernel module
#

%if %{build_modules}

%if 0%{!?kernel_version:1}
%if 0%{?kernvers:1}
# kernvers is the deprecated name for kernel_version.
%global kernel_version %{kernvers}
%else
%global kernel_version %(uname -r)
%endif
%endif

# The path to the kernel headers for the target kernel version.
%if 0%{!?kernel_headers:1}
%global kernel_headers %{_usrsrc}/kernels/%{kernel_version}
%endif

# The kernel modules installation path.
%if 0%{!?kernel_modules:1}
%global kernel_modules %{_prefix}/lib/modules/%{kernel_version}
%endif

%endif

#
# Installation paths
#

# Define the location to the legacy workstation directory.
%global afswsdir /usr/afsws

%if %{with kauth}
# Define the location of the PAM security module directory
%global pamdir %{_libdir}/security
%endif

#
# rpm build settings
#

# Disable setting the source_date_epoch from the top entry of the changelog and
# instead use the current system time by default.
#
# Note: Downstream packagers which use this reference spec are encouraged
#       to set source_date_epoch_from_changelog to 1 and update the changelog
#       on each release to set the source_date_epoch.
%global source_date_epoch_from_changelog 0

%if 0%{!?source_date_epoch:1}
%global source_date_epoch %(date +%%s)
%endif

# Disable using lto (link time optimization)
%global _lto_cflags %{nil}

#-----------------------------------------------------------------------------
# Packages
#-----------------------------------------------------------------------------
%global openafs_description %{expand:OpenAFS is an open source implementation of the AFS distributed
filesystem, which allows cross-platform sharing of files among multiple
computers. Facilities are provided for access control, authentication,
backup, and administrative management.
}

Summary: OpenAFS distributed filesystem
Name: openafs
Version: %{package_version}
Release: %{package_release}%{?dist}
License: IBM Public License
URL: https://www.openafs.org
BuildRequires: pam-devel
BuildRequires: ncurses-devel
BuildRequires: make
BuildRequires: flex
BuildRequires: bison
BuildRequires: systemd-rpm-macros
BuildRequires: perl-devel
BuildRequires: swig
BuildRequires: perl(ExtUtils::Embed)
BuildRequires: perl-core
%if %{with krb5}
BuildRequires: krb5-devel
%endif
%if %{build_userspace}
BuildRequires: perl(Test::More)
%endif

ExclusiveArch: %{ix86} x86_64 ppc64 ppc64le s390x aarch64

Source0: https://www.openafs.org/dl/openafs/%{openafs_version}/openafs-%{openafs_version}-src.tar.bz2
Source10: https://www.openafs.org/dl/openafs/%{openafs_version}/RELNOTES-%{openafs_version}
Source11: https://www.openafs.org/dl/openafs/%{openafs_version}/ChangeLog
Source20: https://www.central.org/dl/cellservdb/CellServDB.2025-08-16
Source30: openafs-cacheinfo
Source32: openafs-client.service
Source33: openafs-client-systemd-helper.sh
Source34: openafs-LICENSE.Sun
Source35: openafs-README
Source37: openafs-server.service
Source38: openafs.sysconfig
Source39: openafs-ThisCell
Source40: openafs-dkms.conf

%description
%{openafs_description}
This package provides the commands, libraries, and configuration files
shared by the OpenAFS client and server packages, including the fs,
pts, tokens, vos, and bos commands.

#
# Userspace packages
#
%if %{build_userspace}

%package client
Summary: OpenAFS Filesystem Client
Requires: %{name} = %{version}
Requires: systemd-units
Requires(post): systemd-units
Requires(preun): systemd-units
Requires(postun): systemd-units
Requires: %{name}-kmod >= %{version}
Provides: %{name}-kmod-common = %{version}

%description client
%{openafs_description}
This package provides basic client support to mount and manipulate
AFS.

%package server
Summary: OpenAFS Filesystem Server
Requires: %{name} = %{version}
Requires: systemd-units
Requires(post): systemd-units
Requires(preun): systemd-units
Requires(postun): systemd-units

%description server
%{openafs_description}
This package provides basic server support to host files in an AFS
cell.

%if %{build_dkmspkg}
%package -n dkms-%{name}
Summary:        DKMS-ready kernel source for OpenAFS distributed filesystem
Provides:       openafs-kernel = %{version}
Provides:       %{name}-kmod = %{version}
Requires(pre):  dkms
Requires(pre):  make
Requires(pre):  flex
Requires(post): dkms
Requires:       %{name}-kmod-common = %{version}

%description -n dkms-%{name}
%{openafs_description}
This package provides the source code to allow DKMS to build an
OpenAFS kernel module.
%endif

%if %{with authlibs}
%package authlibs
Summary: OpenAFS authentication shared libraries

%description authlibs
%{openafs_description}
This package provides the libafsauthent, libafsrpc, and libkopenafs
shared libraries, for third-party software that performs AFS
authentication or makes AFS RPC calls.
%endif

%package authlibs-devel
Summary: OpenAFS shared library development
%if %{with authlibs}
Requires: %{name}-authlibs = %{version}-%{release}
%endif
Requires: %{name}-devel = %{version}-%{release}

%description authlibs-devel
%{openafs_description}
This package provides the static libraries, the development symlinks
for the libafsauthent, libafsrpc, and libkopenafs shared libraries,
and the kopenafs.h header, needed to build programs that use the
OpenAFS authentication libraries.

%package devel
Summary: OpenAFS Development Libraries and Headers
Requires: %{name} = %{version}-%{release}

%description devel
%{openafs_description}
This package provides the headers, static libraries, and tools such as
rxgen and afs_compile_et, needed to build programs that use the OpenAFS
libraries. It also provides the development files for the libafshcrypto
and librokenafs shared libraries. The development files for the
libafsauthent, libafsrpc, and libkopenafs shared libraries are in the
openafs-authlibs-devel package.

%package docs
Summary: OpenAFS user and administrator documentation
Requires: %{name} = %{version}-%{release}
%if %{build_userspace}
BuildRequires: libxslt
BuildRequires: docbook-style-xsl
BuildRequires: docbook-dtds
%endif

%description docs
%{openafs_description}
This package provides HTML documentation for OpenAFS users and system
administrators.

%package kernel-source
Summary: OpenAFS Kernel Module source tree
Provides: openafs-kernel = %{version}
Provides: %{name}-kmod = %{version}

%description kernel-source
%{openafs_description}
This package provides the source code to build your own OpenAFS kernel
module.

%package compat
Summary: OpenAFS client compatibility symlinks
Requires: %{name} = %{version}

%description compat
%{openafs_description}
This package provides compatibility symlinks in %{afswsdir}.  It is
completely optional, and is only necessary to support legacy
applications and scripts that hard-code the location of OpenAFS client
programs.

%if %{with kauth}
%package kauth-client
Summary: OpenAFS Kauth Client support
Requires: %{name}

%description kauth-client
%{openafs_description}
This package provides the legacy KAServer client programs and the PAM module
for authentication with the OpenAFS KAserver; a deprecated authentication
service.  Generally you should not install this package for new cells or for
cells using Kerberos v5.

%package kauth-server
Summary: OpenAFS Kauth Server support
Requires: %{name}

%description kauth-server
%{openafs_description}
This package provides the legacy OpenAFS KAServer; a deprecated authentication
service. Generally you should not install this package for new cells or for
cells using Kerberos v5.
%endif

%if %{with krb5}
%package krb5
Summary: OpenAFS programs for Kerberos 5 authentication
Requires: %{name} = %{version}

%description krb5
%{openafs_description}
This package provides the programs for using Kerberos 5 with AFS,
including aklog, which obtains AFS tokens from Kerberos 5 tickets, and
asetkey and akeyconvert, which add keys from a Kerberos 5 keytab to the
server key files.
%endif

%endif

#
# Kernel module packages
#
%if %{build_modules}

# Internal: kernel_version without the architecture suffix for the package
# metadata.
%global kverrel %(echo %{kernel_version} | sed -e 's/\.%{_target_cpu}$//')

# Internal: Amazon Linux 2023 and later kernel packages have epoch 1.
%if 0%{?amzn} >= 2023
%global kepoch 1
%endif

%package -n kmod-%{name}
Summary:          OpenAFS kernel module
Provides:         %{name}-kmod = %{version}-%{release}
Provides:         openafs-kernel = %{version}
Requires:         kernel-%{_target_cpu} = %{?kepoch:%{kepoch}:}%{kverrel}
Requires:         %{name}-kmod-common >= %{version}
Requires(post):   /usr/sbin/depmod
Requires(postun): /usr/sbin/depmod
Release:          %{package_release}.%(echo %{kverrel} | tr - _)
BuildRequires:    kernel-devel-%{_target_cpu} = %{?kepoch:%{kepoch}:}%{kverrel}
BuildRequires:    elfutils-devel

%description -n kmod-%{name}
%{openafs_description}
This package provides the OpenAFS kernel module built for the Linux
kernel %{kernel_version}.

%endif

#-----------------------------------------------------------------------------
# Prep
#-----------------------------------------------------------------------------
%prep

: @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
: @@@
%if %{build_modules}
: @@@ kernel version:     %{kernel_version}
%endif
%if %{with kauth}
: @@@ PAM modules dir:    %{pamdir}
%endif
: @@@ build userspace:    %{build_userspace}
: @@@ build modules:      %{build_modules}
: @@@ arch:               %{_arch}
: @@@
: @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

# Unpack the OpenAFS source tree.
%setup -q -n openafs-%{openafs_version}

# Add the change log and release notes to source tree.
cp -p %{SOURCE10} .
cp -p %{SOURCE11} .

#-----------------------------------------------------------------------------
# Build
#-----------------------------------------------------------------------------
%build

export SOURCE_DATE_EPOCH=%{source_date_epoch}
export CFLAGS="$RPM_OPT_FLAGS"

./configure \
       --prefix=%{_prefix} \
       --libdir=%{_libdir} \
       --bindir=%{_bindir} \
       --sbindir=%{_sbindir} \
       --disable-strip-binaries \
       --enable-debug \
       --enable-transarc-paths \
       --disable-fuse-client \
       --with-linux-kernel-packaging \
%if %{build_modules}
       --enable-kernel-module \
       --with-linux-kernel-headers=%{kernel_headers} \
%else
       --disable-kernel-module \
%endif
%if %{with krb5}
       --with-krb5 \
%endif
       --with-swig \
%if %{with kauth}
       --enable-kauth \
%endif
%if %{with supergroups}
       --enable-supergroups \
%endif

make %{_smp_mflags} only_libafs_tree

%if %{build_userspace}
make %{_smp_mflags} all_nolibafs
make %{_smp_mflags} -C doc/xml/UserGuide index.html
make %{_smp_mflags} -C doc/xml/AdminGuide index.html
make %{_smp_mflags} -C doc/xml/QuickStartUnix index.html
%endif

%if %{build_modules}
make %{_smp_mflags} libafs
%endif

#-----------------------------------------------------------------------------
# Install
#-----------------------------------------------------------------------------
%install

export SOURCE_DATE_EPOCH=%{source_date_epoch}

%if %{build_userspace}

#
# Install userspace files
#
make %{_smp_mflags} install_nolibafs DESTDIR="%{buildroot}"

# Shared libraries are conventionally installed executable on Red Hat
# systems, and rpm tooling such as find-debuginfo relies on this. The
# OpenAFS build system installs them without the executable bit, so set
# it here.
chmod +x %{buildroot}%{_libdir}/*.so*

# Exclude duplicated files.
rm -f %{buildroot}%{_prefix}/afs/bin/bos
rm -f %{buildroot}%{_prefix}/afs/bin/fs
%if %{with kauth}
rm -f %{buildroot}%{_prefix}/afs/bin/kas
rm -f %{buildroot}%{_prefix}/afs/bin/klog
rm -f %{buildroot}%{_prefix}/afs/bin/klog.krb
rm -f %{buildroot}%{_prefix}/afs/bin/kpwvalid
rm -f %{buildroot}%{_sbindir}/kpwvalid
%endif
rm -f %{buildroot}%{_prefix}/afs/bin/pts
rm -f %{buildroot}%{_prefix}/afs/bin/tokens
rm -f %{buildroot}%{_prefix}/afs/bin/tokens.krb
rm -f %{buildroot}%{_prefix}/afs/bin/udebug
rm -f %{buildroot}%{_prefix}/afs/bin/vos

# Relocate admin utilities to a modern path.
%if %{with kauth}
mv %{buildroot}%{_prefix}/afs/bin/kadb_check %{buildroot}%{_sbindir}/kadb_check
%endif
mv %{buildroot}%{_prefix}/afs/bin/prdb_check %{buildroot}%{_sbindir}/prdb_check
mv %{buildroot}%{_prefix}/afs/bin/vldb_check %{buildroot}%{_sbindir}/vldb_check
mv %{buildroot}%{_prefix}/afs/bin/vldb_convert %{buildroot}%{_sbindir}/vldb_convert
%if %{with krb5}
mv %{buildroot}%{_prefix}/afs/bin/akeyconvert %{buildroot}%{_sbindir}/akeyconvert
mv %{buildroot}%{_prefix}/afs/bin/asetkey %{buildroot}%{_sbindir}/asetkey
%endif

%if %{with kauth}
# Relocate PAM files to the standard PAM module path.
mkdir -p %{buildroot}%{pamdir}
mv %{buildroot}%{_libdir}/pam_afs.krb.so %{buildroot}%{pamdir}
mv %{buildroot}%{_libdir}/pam_afs.so %{buildroot}%{pamdir}
ln -sf pam_afs.so %{buildroot}%{pamdir}/pam_afs.so.1
ln -sf pam_afs.krb.so %{buildroot}%{pamdir}/pam_afs.krb.so.1

# Rename kpasswd to avoid conflicting with krb5 kpasswd.
mv %{buildroot}%{_bindir}/kpasswd %{buildroot}%{_bindir}/kapasswd
mv %{buildroot}%{_mandir}/man1/kpasswd.1 %{buildroot}%{_mandir}/man1/kapasswd.1
%endif

#
# Install client and server systemd files
#
mkdir -p %{buildroot}%{_libexecdir}/%{name}
mkdir -p %{buildroot}%{_sysconfdir}/sysconfig
install -m 644 %{SOURCE38} %{buildroot}%{_sysconfdir}/sysconfig/openafs
mkdir -p %{buildroot}%{_unitdir}
install -m 644 %{SOURCE32} %{buildroot}%{_unitdir}/openafs-client.service
install -m 644 %{SOURCE37} %{buildroot}%{_unitdir}/openafs-server.service
install -m 755 %{SOURCE33} %{buildroot}%{_libexecdir}/%{name}/openafs-client-systemd-helper.sh

#
# Install server directories
#
mkdir -p %{buildroot}%{_prefix}/afs/etc
mkdir -p %{buildroot}%{_prefix}/afs/logs

#
# Install client directories and config files
#
mkdir -p %{buildroot}%{_prefix}/vice/etc
mkdir -p %{buildroot}%{_prefix}/vice/cache
chmod 700 %{buildroot}%{_prefix}/vice/cache
install -p -m 644 %{SOURCE39} %{buildroot}%{_prefix}/vice/etc/ThisCell
touch %{buildroot}%{_prefix}/vice/etc/CellServDB.local
install -p -m 644 %{SOURCE20} %{buildroot}%{_prefix}/vice/etc/CellServDB.dist
install -p -m 644 %{SOURCE30} %{buildroot}%{_prefix}/vice/etc/cacheinfo

# Compatibility symlink for the legacy afsd path.
ln -s ../../sbin/afsd %{buildroot}%{_prefix}/vice/etc/afsd

#
# Install DKMS source
#
install -d -m 755 %{buildroot}%{_prefix}/src
cp -a libafs_tree %{buildroot}%{_prefix}/src/%{name}-%{dkms_version}
sed -e 's/@DKMS_VERSION@/%{dkms_version}/g' %{SOURCE40} \
    > %{buildroot}%{_prefix}/src/%{name}-%{dkms_version}/dkms.conf

#
# Install the kernel module source tree
#
mkdir -p %{buildroot}%{_prefix}/src/openafs-kernel-%{openafs_version}/src
tar cf - -C libafs_tree . | \
    tar xf - -C %{buildroot}%{_prefix}/src/openafs-kernel-%{openafs_version}/src
install -m 644 LICENSE %{buildroot}%{_prefix}/src/openafs-kernel-%{openafs_version}/LICENSE.IBM
install -m 644 %{SOURCE34} %{buildroot}%{_prefix}/src/openafs-kernel-%{openafs_version}/LICENSE.Sun
install -m 644 %{SOURCE35} %{buildroot}%{_prefix}/src/openafs-kernel-%{openafs_version}/README

#
# Install documentation
#
mkdir -p %{buildroot}%{_docdir}/%{name}/html
cp -p doc/xml/home.gif doc/xml/next.gif doc/xml/prev.gif doc/xml/up.gif \
    %{buildroot}%{_docdir}/%{name}/html
for guide in UserGuide AdminGuide QuickStartUnix; do
    mkdir -p %{buildroot}%{_docdir}/%{name}/html/$guide
    cp -p doc/xml/$guide/*.html doc/xml/$guide/*.png \
        %{buildroot}%{_docdir}/%{name}/html/$guide
done
cp -pr doc/pdf %{buildroot}%{_docdir}/%{name}/pdf

%endif

#
# Install the kernel module
#
%if %{build_modules}

mkdir -p %{buildroot}%{kernel_modules}/extra/openafs
install -m 755 \
    src/libafs/MODLOAD-%{kernel_version}/openafs.ko \
    %{buildroot}%{kernel_modules}/extra/openafs/openafs.ko

%endif

#-----------------------------------------------------------------------------
# Check
#-----------------------------------------------------------------------------
%check
%if %{build_userspace}
make -C tests check
%endif

#-----------------------------------------------------------------------------
# Scriptlets
#-----------------------------------------------------------------------------
%if %{build_userspace}

# package openafs
%preun
if [ $1 = 0 ] ; then
    if [ -d /afs ]; then
        rmdir /afs || :
    fi
fi

# package openafs-client
%post client
if [ ! -d /afs ]; then
    mkdir /afs
    chown root:root /afs
    chmod 0555 /afs
    [ -x /sbin/restorecon ] && /sbin/restorecon /afs
fi

# Create the CellServDB from the local and distributed CellServDB files.
if [ -h %{_prefix}/vice/etc/CellServDB ]; then
    rm -f %{_prefix}/vice/etc/CellServDB
fi
cat %{_prefix}/vice/etc/CellServDB.local %{_prefix}/vice/etc/CellServDB.dist \
    > %{_prefix}/vice/etc/CellServDB
chmod 644 %{_prefix}/vice/etc/CellServDB

%systemd_post openafs-client.service

%preun client
%systemd_preun openafs-client.service

%postun client
%systemd_postun openafs-client.service

# package openafs-server
%post server
%systemd_post openafs-server.service

%preun server
%systemd_preun openafs-server.service

%postun server
%systemd_postun openafs-server.service

# package dkms-openafs
%if %{build_dkmspkg}
%post -n dkms-%{name}
dkms add -m %{name} -v %{dkms_version} --rpm_safe_upgrade
dkms build -m %{name} -v %{dkms_version} --rpm_safe_upgrade
dkms install -m %{name} -v %{dkms_version} --rpm_safe_upgrade

%preun -n dkms-%{name}
dkms remove -m %{name} -v %{dkms_version} --rpm_safe_upgrade --all ||:
%endif

# package openafs-compat
%post compat
# Create compatiblity links.
mkdir -p %{afswsdir}/bin
mkdir -p %{afswsdir}/etc
ln -sf %{_bindir}/afsmonitor    %{afswsdir}/bin/afsmonitor
ln -sf %{_bindir}/bos           %{afswsdir}/bin/bos
ln -sf %{_bindir}/fs            %{afswsdir}/bin/fs
ln -sf %{_bindir}/pagsh         %{afswsdir}/bin/pagsh
ln -sf %{_bindir}/pts           %{afswsdir}/bin/pts
ln -sf %{_bindir}/restorevol    %{afswsdir}/bin/restorevol
ln -sf %{_bindir}/scout         %{afswsdir}/bin/scout
ln -sf %{_bindir}/sys           %{afswsdir}/bin/sys
ln -sf %{_bindir}/tokens        %{afswsdir}/bin/tokens
ln -sf %{_bindir}/translate_et  %{afswsdir}/bin/translate_et
ln -sf %{_bindir}/xstat_cm_test %{afswsdir}/bin/xstat_cm_test
ln -sf %{_bindir}/xstat_fs_test %{afswsdir}/bin/xstat_fs_test
ln -sf %{_bindir}/udebug        %{afswsdir}/bin/udebug
ln -sf %{_bindir}/unlog         %{afswsdir}/bin/unlog
ln -sf %{_sbindir}/backup       %{afswsdir}/etc/backup
ln -sf %{_sbindir}/butc         %{afswsdir}/etc/butc
ln -sf %{_sbindir}/fms          %{afswsdir}/etc/fms
ln -sf %{_sbindir}/fstrace      %{afswsdir}/etc/fstrace
ln -sf %{_sbindir}/read_tape    %{afswsdir}/etc/read_tape
ln -sf %{_sbindir}/rxdebug      %{afswsdir}/etc/rxdebug
ln -sf %{_sbindir}/uss          %{afswsdir}/etc/uss
ln -sf %{_sbindir}/vos          %{afswsdir}/etc/vos
ln -sf %{_sbindir}/vsys         %{afswsdir}/etc/vsys

%postun compat
if [ $1 = 0 ] ; then
    rmdir %{afswsdir}/bin >/dev/null 2>/dev/null || :
    rmdir %{afswsdir}/etc >/dev/null 2>/dev/null || :
    rmdir %{afswsdir} >/dev/null 2>/dev/null || :
fi

# package openafs-kauth-client
%if %{with kauth}
%post kauth-client
# Create compatiblity links.
mkdir -p %{afswsdir}/bin
mkdir -p %{afswsdir}/etc
ln -sf %{_bindir}/kapasswd      %{afswsdir}/bin/kapasswd
ln -sf %{_bindir}/klog          %{afswsdir}/bin/klog
ln -sf %{_bindir}/klog.krb      %{afswsdir}/bin/klog.krb
ln -sf %{_sbindir}/kas          %{afswsdir}/etc/kas
ln -sf %{_bindir}/pagsh.krb     %{afswsdir}/bin/pagsh.krb
ln -sf %{_bindir}/tokens.krb    %{afswsdir}/bin/tokens.krb

%postun kauth-client
if [ $1 = 0 ] ; then
    rmdir %{afswsdir}/bin >/dev/null 2>/dev/null || :
    rmdir %{afswsdir}/etc >/dev/null 2>/dev/null || :
    rmdir %{afswsdir} >/dev/null 2>/dev/null || :
fi
%endif

# end of build_userspace
%endif

# package kmod-openafs
%if %{build_modules}
%post -n kmod-%{name}
/usr/sbin/depmod -aeF /boot/System.map-%{kernel_version} %{kernel_version} > /dev/null || :

%postun -n kmod-%{name}
/usr/sbin/depmod -aF /boot/System.map-%{kernel_version} %{kernel_version} &> /dev/null || :
%endif

#-----------------------------------------------------------------------------
# Files
#-----------------------------------------------------------------------------
%if %{build_userspace}

%files
%config(noreplace) %{_sysconfdir}/sysconfig/openafs
%license LICENSE
%exclude %{_docdir}/%{name}/LICENSE
%doc ChangeLog
%doc RELNOTES-%{openafs_version}
%dir %{_libexecdir}/%{name}
%{_bindir}/afsmonitor
%{_bindir}/bos
%{_bindir}/fs
%{_bindir}/pagsh
%{_bindir}/pts
%{_bindir}/restorevol
%{_bindir}/scout
%{_bindir}/sys
%{_bindir}/tokens
%{_bindir}/translate_et
%{_bindir}/udebug
%{_bindir}/unlog
%{_bindir}/xstat_cm_test
%{_bindir}/xstat_fs_test
%{_libdir}/libafshcrypto.so.*
%{_libdir}/librokenafs.so.*
%{_mandir}/man1/afs.1*
%{_mandir}/man1/afsmonitor.1*
%{_mandir}/man1/fs.1*
%{_mandir}/man1/fs_*.1*
%{_mandir}/man1/pagsh.1*
%{_mandir}/man1/pts.1*
%{_mandir}/man1/pts_*.1*
%{_mandir}/man1/restorevol.1*
%{_mandir}/man1/rxdebug.1*
%{_mandir}/man1/scout.1*
%{_mandir}/man1/sys.1*
%{_mandir}/man1/tokens.1*
%{_mandir}/man1/translate_et.1*
%{_mandir}/man1/udebug.1*
%{_mandir}/man1/unlog.1*
%{_mandir}/man1/vos.1*
%{_mandir}/man1/vos_*.1*
%{_mandir}/man1/xstat_cm_test.1*
%{_mandir}/man1/xstat_fs_test.1*
%{_mandir}/man5/CellServDB.5*
%{_mandir}/man5/ThisCell.5*
%{_mandir}/man5/afsmonitor.5*
%{_mandir}/man5/butc.5*
%{_mandir}/man5/butc_logs.5*
%{_mandir}/man5/uss.5*
%{_mandir}/man5/uss_bulk.5*
%{_mandir}/man8/backup.8*
%{_mandir}/man8/backup_*.8*
%{_mandir}/man8/bos.8*
%{_mandir}/man8/bos_*.8*
%{_mandir}/man8/butc.8*
%{_mandir}/man8/fms.8*
%{_mandir}/man8/fstrace.8*
%{_mandir}/man8/fstrace_*.8*
%{_mandir}/man8/read_tape.8*
%{_mandir}/man8/uss.8*
%{_mandir}/man8/uss_*.8*
%{_mandir}/man8/vsys.8*
%{_sbindir}/backup
%{_sbindir}/butc
%{_sbindir}/fms
%{_sbindir}/fstrace
%{_sbindir}/read_tape
%{_sbindir}/rxdebug
%{_sbindir}/rxstat_clear_peer
%{_sbindir}/rxstat_clear_process
%{_sbindir}/rxstat_disable_peer
%{_sbindir}/rxstat_disable_process
%{_sbindir}/rxstat_enable_peer
%{_sbindir}/rxstat_enable_process
%{_sbindir}/rxstat_get_peer
%{_sbindir}/rxstat_get_process
%{_sbindir}/rxstat_get_version
%{_sbindir}/rxstat_query_peer
%{_sbindir}/rxstat_query_process
%{_sbindir}/uss
%{_sbindir}/vos
%{_sbindir}/vsys
# Exclude obsolete or unused files.
%exclude %{_bindir}/livesys
%exclude %{_mandir}/man1/dlog.1*
%exclude %{_mandir}/man1/livesys.1*
%exclude %{_mandir}/man1/symlink.1*
%exclude %{_mandir}/man1/symlink_list.1*
%exclude %{_mandir}/man1/symlink_make.1*
%exclude %{_mandir}/man1/symlink_remove.1*
%exclude %{_mandir}/man8/aklog_dynamic_auth.8*
%exclude %{_mandir}/man8/rmtsysd.8*
%exclude %{_mandir}/man8/xfs_size_check.8*
%exclude %{_sbindir}/rmtsysd
%if %{without authlibs}
%exclude %{_libdir}/libafsauthent.so*
%exclude %{_libdir}/libafsrpc.so*
%exclude %{_libdir}/libkopenafs.so*
%endif
%if %{without kauth}
%exclude %{_bindir}/pagsh.krb
%exclude %{_bindir}/tokens.krb
%exclude %{_mandir}/man5/AuthLog.5*
%exclude %{_mandir}/man5/AuthLog.dir.5*
%endif
%if %{without krb5}
%exclude %{_mandir}/man8/akeyconvert.8*
%exclude %{_mandir}/man8/asetkey.8*
%endif

%files docs
%{_docdir}/%{name}/html
%{_docdir}/%{name}/pdf

%files client
%dir %{_prefix}/vice
%dir %{_prefix}/vice/cache
%dir %{_prefix}/vice/etc
%dir %{_prefix}/vice/etc/C
%{_prefix}/vice/etc/CellServDB.dist
%config(noreplace) %{_prefix}/vice/etc/CellServDB.local
%config(noreplace) %{_prefix}/vice/etc/ThisCell
%config(noreplace) %{_prefix}/vice/etc/cacheinfo
%{_bindir}/afsio
%{_bindir}/cmdebug
%{_bindir}/up
%{_libdir}/libuafs.a
%{_libdir}/libuafs_pic.a
%{_prefix}/vice/etc/C/afszcm.cat
%{_prefix}/vice/etc/afsd
%{_sbindir}/afsd
%{_libexecdir}/%{name}/openafs-client-systemd-helper.sh
%{_unitdir}/openafs-client.service
%{_mandir}/man1/cmdebug.1*
%{_mandir}/man1/up.1*
%{_mandir}/man5/CellAlias.5*
%{_mandir}/man5/afs.5*
%{_mandir}/man5/afs_cache.5*
%{_mandir}/man5/afs_volume_header.5*
%{_mandir}/man5/afszcm.cat.5*
%{_mandir}/man5/cacheinfo.5*
%{_mandir}/man8/afsd.8*

%files server
%dir %{_prefix}/afs
%dir %{_prefix}/afs/bin
%dir %{_prefix}/afs/etc
%dir %{_prefix}/afs/logs
%{_prefix}/afs/bin/bos_util
%{_prefix}/afs/bin/bosserver
%{_prefix}/afs/bin/buserver
%{_prefix}/afs/bin/dafileserver
%{_prefix}/afs/bin/dafssync-debug
%{_prefix}/afs/bin/dasalvager
%{_prefix}/afs/bin/davolserver
%{_prefix}/afs/bin/fileserver
%{_prefix}/afs/bin/fssync-debug
%{_prefix}/afs/bin/pt_util
%{_prefix}/afs/bin/ptserver
%{_prefix}/afs/bin/salvager
%{_prefix}/afs/bin/salvageserver
%{_prefix}/afs/bin/salvsync-debug
%{_prefix}/afs/bin/state_analyzer
%{_prefix}/afs/bin/upclient
%{_prefix}/afs/bin/upserver
%{_prefix}/afs/bin/vlserver
%{_prefix}/afs/bin/volinfo
%{_prefix}/afs/bin/volscan
%{_prefix}/afs/bin/volserver
%{_sbindir}/prdb_check
%{_sbindir}/vldb_check
%{_sbindir}/vldb_convert
%{_sbindir}/voldump
%{_unitdir}/openafs-server.service
%{_mandir}/man5/BackupLog.5*
%{_mandir}/man5/BosConfig.5*
%{_mandir}/man5/BosLog.5*
%{_mandir}/man5/FORCESALVAGE.5*
%{_mandir}/man5/FileLog.5*
%{_mandir}/man5/KeyFile.5*
%{_mandir}/man5/KeyFileExt.5*
%{_mandir}/man5/NetInfo.5*
%{_mandir}/man5/NetRestrict.5*
%{_mandir}/man5/NoAuth.5*
%{_mandir}/man5/PtLog.5*
%{_mandir}/man5/SALVAGE.fs.5*
%{_mandir}/man5/SalvageLog.5*
%{_mandir}/man5/UserList.5*
%{_mandir}/man5/VLLog.5*
%{_mandir}/man5/VolserLog.5*
%{_mandir}/man5/bdb.DB0.5*
%{_mandir}/man5/fms.log.5*
%{_mandir}/man5/krb.conf.5*
%{_mandir}/man5/krb.excl.5*
%{_mandir}/man5/prdb.DB0.5*
%{_mandir}/man5/salvage.lock.5*
%{_mandir}/man5/sysid.5*
%{_mandir}/man5/tapeconfig.5*
%{_mandir}/man5/vldb.DB0.5*
%{_mandir}/man8/bosserver.8*
%{_mandir}/man8/buserver.8*
%{_mandir}/man8/dafileserver.8*
%{_mandir}/man8/dafssync-debug.8*
%{_mandir}/man8/dafssync-debug_*.8*
%{_mandir}/man8/dasalvager.8*
%{_mandir}/man8/davolserver.8*
%{_mandir}/man8/fileserver.8*
%{_mandir}/man8/fssync-debug.8*
%{_mandir}/man8/fssync-debug_*.8*
%{_mandir}/man8/prdb_check.8*
%{_mandir}/man8/pt_util.8*
%{_mandir}/man8/ptserver.8*
%{_mandir}/man8/salvager.8*
%{_mandir}/man8/salvageserver.8*
%{_mandir}/man8/state_analyzer.8*
%{_mandir}/man8/upclient.8*
%{_mandir}/man8/upserver.8*
%{_mandir}/man8/vldb_check.8*
%{_mandir}/man8/vldb_convert.8*
%{_mandir}/man8/vlserver.8*
%{_mandir}/man8/voldump.8*
%{_mandir}/man8/volinfo.8*
%{_mandir}/man8/volscan.8*
%{_mandir}/man8/volserver.8*

%if %{with authlibs}
%files authlibs
%{_libdir}/libafsauthent.so.*
%{_libdir}/libafsrpc.so.*
%{_libdir}/libkopenafs.so.*
%endif

%files authlibs-devel
%{_includedir}/kopenafs.h
%{_libdir}/libafsauthent.a
%{_libdir}/libafsauthent_pic.a
%{_libdir}/libafscp.a
%{_libdir}/libafsrpc.a
%{_libdir}/libafsrpc_pic.a
%{_libdir}/libkopenafs.a
%if %{with authlibs}
%{_libdir}/libafsauthent.so
%{_libdir}/libafsrpc.so
%{_libdir}/libkopenafs.so
%endif

%files devel
%dir %{_includedir}/opr
%{_bindir}/afs_compile_et
%{_bindir}/rxgen
%{_includedir}/afs
%{_includedir}/lwp.h
%{_includedir}/opr/lock.h
%{_includedir}/opr/queue.h
%{_includedir}/rx
%{_includedir}/ubik.h
%{_includedir}/ubik_int.h
%{_libdir}/afs
%{_libdir}/libafshcrypto.a
%{_libdir}/libafshcrypto.so
%{_libdir}/libafsrfc3961.a
%{_libdir}/liblwp.a
%{_libdir}/libopr.a
%{_libdir}/librokenafs.a
%{_libdir}/librokenafs.so
%{_libdir}/librx.a
%{_libdir}/librxkad.a
%{_libdir}/librxstat.a
%{_libdir}/libubik.a
%{_mandir}/man1/afs_compile_et.1*
%{_mandir}/man1/rxgen.1*
%dir %{_libdir}/perl
%dir %{_libdir}/perl/AFS
%{_libdir}/perl/AFS/ukernel.pm
%{_libdir}/perl/ukernel.so
%{_mandir}/man3/AFS::ukernel.3*

%if %{build_dkmspkg}
%files -n dkms-%{name}
%{_prefix}/src/%{name}-%{dkms_version}
%endif

%files kernel-source
%{_prefix}/src/openafs-kernel-%{openafs_version}

%files compat
%ghost %{afswsdir}/bin/afsmonitor
%ghost %{afswsdir}/bin/bos
%ghost %{afswsdir}/bin/fs
%ghost %{afswsdir}/bin/pagsh
%ghost %{afswsdir}/bin/pts
%ghost %{afswsdir}/bin/restorevol
%ghost %{afswsdir}/bin/scout
%ghost %{afswsdir}/bin/sys
%ghost %{afswsdir}/bin/tokens
%ghost %{afswsdir}/bin/translate_et
%ghost %{afswsdir}/bin/udebug
%ghost %{afswsdir}/bin/unlog
%ghost %{afswsdir}/bin/xstat_cm_test
%ghost %{afswsdir}/bin/xstat_fs_test
%ghost %{afswsdir}/etc/backup
%ghost %{afswsdir}/etc/butc
%ghost %{afswsdir}/etc/fms
%ghost %{afswsdir}/etc/fstrace
%ghost %{afswsdir}/etc/read_tape
%ghost %{afswsdir}/etc/rxdebug
%ghost %{afswsdir}/etc/uss
%ghost %{afswsdir}/etc/vos
%ghost %{afswsdir}/etc/vsys

%if %{with kauth}
%files kauth-client
%{_bindir}/kapasswd
%{_bindir}/klog
%{_bindir}/klog.krb
%{_bindir}/kpwvalid
%{_bindir}/pagsh.krb
%{_bindir}/tokens.krb
%{_mandir}/man1/kapasswd.1*
%{_mandir}/man1/klog.1*
%{_mandir}/man1/klog.krb.1*
%{_mandir}/man1/pagsh.krb.1*
%{_mandir}/man1/tokens.krb.1*
%{_mandir}/man8/kas.8*
%{_mandir}/man8/kas_*.8*
%{_mandir}/man8/kpwvalid.8*
%{_sbindir}/kas
%{pamdir}/pam_afs.krb.so
%{pamdir}/pam_afs.krb.so.1
%{pamdir}/pam_afs.so
%{pamdir}/pam_afs.so.1
%ghost %{afswsdir}/bin/kapasswd
%ghost %{afswsdir}/bin/klog
%ghost %{afswsdir}/bin/klog.krb
%ghost %{afswsdir}/bin/pagsh.krb
%ghost %{afswsdir}/bin/tokens.krb
%ghost %{afswsdir}/etc/kas
%exclude %{_bindir}/knfs
%exclude %{_mandir}/man1/knfs.1*

%files kauth-server
%{_mandir}/man5/AuthLog.5*
%{_mandir}/man5/AuthLog.dir.5*
%{_mandir}/man5/kaserver.DB0.5*
%{_mandir}/man5/kaserverauxdb.5*
%{_mandir}/man8/ka-forwarder.8*
%{_mandir}/man8/kadb_check.8*
%{_mandir}/man8/kaserver.8*
%{_prefix}/afs/bin/ka-forwarder
%{_prefix}/afs/bin/kaserver
%{_sbindir}/kadb_check
%exclude %{_mandir}/man8/kdb.8*
%exclude %{_prefix}/afs/bin/kdb
%endif

%if %{with krb5}
%files krb5
%{_bindir}/aklog
%{_bindir}/klog.krb5
%{_mandir}/man1/aklog.1*
%{_mandir}/man1/klog.krb5.1*
%{_mandir}/man8/akeyconvert.8*
%{_mandir}/man8/asetkey.8*
%{_sbindir}/akeyconvert
%{_sbindir}/asetkey
%endif
%endif

%if %{build_modules}

%files -n kmod-%{name}
%defattr(644,root,root,755)
%{kernel_modules}/extra/%{name}/

%endif

#-----------------------------------------------------------------------------
# Changelog
#-----------------------------------------------------------------------------
%changelog
* Sun Nov 08 2026  Michael Meffie <mmeffie@sinenomine.net>
- Raised the minimum releases to RHEL 8, Fedora 30 and Amazon Linux
  2023; RHEL 7 is now built with openafs.el7.spec.

* Sun Nov 08 2026  Michael Meffie <mmeffie@sinenomine.net>
- Raised the minimum releases to RHEL 7 and Amazon Linux 2; RHEL 6 is
  now built with openafs.el6.spec.

* Sat Nov 07 2026  Michael Meffie <mmeffie@sinenomine.net>
- Many updates since 2007 for newer RHEL, Fedora and Amazon Linux
  releases; see the OpenAFS git history for details. Earlier packaging
  by Derek Atkins, Derrick Brashear and Simon Wilkinson, with
  contributions from David Howells and Nalin Dahyabhai of Red Hat,
  Lee Damon, Alexander Bergolth, Mike Polek and wingc@engin.umich.edu.

* Mon Apr 27 2026  Michael Meffie <mmeffie@sinenomine.net>
- Added Amazon Linux support
