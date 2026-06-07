# OpenAFS Platform Support

## Introduction

This document records the platform support policy for the OpenAFS project to
provide a clear understanding of which platforms are actively maintained.  This
policy helps focus development resources and provides transparency to the user
community.

The OpenAFS project categorizes platform support into the following tiers:

**Tier 1: Maintained**

Tier 1 platforms are currently supported platforms.  The OpenAFS project
focuses its development and testing resources on these platforms.  The OpenAFS
project is committed to addressing bugs reported on Tier 1 platforms in a
timely manner, providing support for new versions of maintained platforms,
and providing fixes in subsequent maintenance releases.

**Tier 2: Best-Effort**

Tier 2 platforms are those for which we provide limited support. While they are
expected to generally work, they may not be a part of our regular testing for
every code change.

The OpenAFS project does not actively develop or test on these platforms and
support often relies on community members with access to the hardware.

Issues on Tier 2 platforms will not prevent a new release.

**Tier 3: Unmaintained**

Tier 3 platforms are not currently actively supported by the OpenAFS project.
While the code for these platforms is currently retained in the source tree, it
may not be known whether OpenAFS runs or even builds on these platforms.

Tier 3 platforms include both platforms that are considered obsolete, and
platforms that are still in use but are not commonly used with OpenAFS. Patches
to improve support for non-obsolete platforms are welcomed (and hopefully
improve them to not be a Tier 3 platform anymore). Patches for obsolete
platforms may be rejected unless it can be shown that there are real users of
that platform.

Code supporting these platforms may be removed from OpenAFS in the future if it
impedes new development.

## Platforms

The support tier for a given platform generally depends on the version of the
operating system. Newer versions of an OS that are still actively maintained by
the vendor will typically be in a higher support tier.  Conversely, older or
obsolete versions of an OS will be in a lower tier.

The following sections outline the specific support tiers by platform.

### Red Hat Enterprise Linux (RHEL)

All versions of RHEL under "Full Support" from Red Hat are a Tier 1 platform.
This currently includes:

- RHEL 10
- RHEL 9
- RHEL 8

All versions of RHEL under "Maintenance"/"Extended" support (not including
"Extended Life Phase") are a Tier 2 platform. This currently includes:

- RHEL 7

All remaining versions of RHEL with Linux kernel 2.6.18 or newer are
obsolete Tier 3 platforms. This currently includes:

- RHEL 6
- RHEL 5

RHEL-derivative platforms are considered equivalent to RHEL, in terms of
platform support. This includes Alma Linux, Rocky Linux, CentOS, and Oracle
Linux.

RPMs for RHEL are possible to build from the OpenAFS source tree by running
`make rpm`. The packaging sources are located in `src/packaging/RedHat`.
Pre-built RPMs are not provided by the OpenAFS Project itself, but may be
obtained from a third party at <https://download.sinenomine.net/openafs/>.

### Fedora Linux

All versions of Fedora Linux supported by the Fedora Project are Tier 1
platforms. Generally this consists of the most recent version of Fedora, and
one previous version.

Older versions of Fedora Linux are Tier 2 if the closest corresponding RHEL
version is Tier 1 or 2.

All other versions of Fedora Linux with Linux kernel 2.6.18 or newer are Tier
3.

RPMs for Fedora are possible to build from the OpenAFS source tree by running
`make rpm`. The packaging sources are located in `src/packaging/RedHat`.
Pre-built RPMs are not provided by the OpenAFS Project itself, but may be
obtained from a third party at <https://download.sinenomine.net/openafs/>.

### SUSE

All versions of SUSE Linux Enterprise (SLE) under "General Support" from SUSE
are a Tier 1 platform. This currently includes:

- SLE 16
- SLE 15

All versions of SLE under "Long Term"/LTSS support are a Tier 2 platform. This
currently includes:

- SLE 12
- SLE 11

SUSE Linux Enterprise Server/Desktop (SLES/SLED) are considered equivalent to
SLE, in terms of platform support.

openSUSE Leap is considered equivalent to the corresponding SLE version.

openSUSE Tumbleweed is a Tier 1 platform.

RPMs for openSUSE can be found as community packages on
<https://software.opensuse.org/package/openafs>.

### Debian/Ubuntu Linux

Debian Linux is a Tier 1 platform for versions currently supported by the
Debian Project. This currently consists of:

- Debian 13 (Trixie)
- Debian 12 (Bookworm)
- Debian 11 (Bullseye)

Debian is a Tier 2 platform for versions under extended support from Debian.
This currently includes:

- Debian 10 (Buster)
- Debian 9 (Stretch)

All remaining versions of Debian with Linux kernel 2.6.18 or newer are
obsolete Tier 3 platforms. This currently includes:

- Debian 8 (Jessie)
- Debian 7 (Wheezy)
- Debian 6 (Squeeze)
- Debian 5 (Lenny)
- Debian 4 (Etch)

Ubuntu is a Tier 1 platform for versions currently under standard support from
Canonical. This currently includes

- Ubuntu 26.04 LTS
- Ubuntu 24.04 LTS
- Ubuntu 22.04 LTS

Ubuntu is a Tier 2 platform for versions under extended support from Canonical.
This currently includes:

- Ubuntu 20.04 LTS
- Ubuntu 18.04 LTS
- Ubuntu 16.04 LTS
- Ubuntu 14.04 LTS

Ubuntu is a Tier 3 platform for all other versions 7.04 and newer.

Official Debian packages for OpenAFS are included in Debian:
<https://packages.debian.org/source/stable/openafs>.

OpenAFS is available in Ubuntu "universe" but does not receive updates to bring
in support for new kernel versions when they are updated in Ubuntu. The
`openafs/stable` PPA at <https://launchpad.net/ubuntu/+source/openafs> makes
the current stable version of OpenAFS available for supported Ubuntu versions,
which includes compatibility with newer kernels as they become available.

### Other Linux

Gentoo Linux is a Tier 1 platform, and packages are available in Gentoo:
<https://packages.gentoo.org/packages/net-fs/openafs>.

Other platforms that use the Linux kernel are generally considered Tier 2, as
long as they use a Linux kernel from around the vintage of a Tier 1 or Tier 2
RHEL platform, and consist of one of the following hardware architectures:

- amd64 (aka `x86_64`)
- arm64
- s390x

Any Linux platforms on these hardware architectures using a Linux kernel older
than Tier 2 RHEL is a Tier 3 platform.

Other Linux platforms consisting of one of the following hardware architectures
are considered an obsolete Tier 3 platform:

- 32-bit arm
- 32-bit x86
- PowerPC
- DEC Alpha
- Itanium

User-mode Linux (UML) is a Tier 3 platform.

### Apple macOS

For OpenAFS client components, macOS is a Tier 1 platform for macOS versions
that are supported by Apple. This generally consists of the most recent version
of macOS and the two previous versions. This currently includes:

- macOS 26 (Tahoe)
- macOS 15 (Sequoia)
- macOS 14 (Sonoma)

macOS is a Tier 2 platform for:

- macOS 13 (Ventura)
- macOS 12 (Monterey)
- macOS 11 (Big Sur)

macOS is a Tier 3 platform for all other versions Mac OS X 10.3 (Panther) and
newer.

For OpenAFS server components, macOS is a Tier 3 platform for all of the above
versions.

Pre-built macOS client packages are not provided by the OpenAFS Project itself,
but may be obtained from a third party at <https://download.sinenomine.net/openafs/>.

### Oracle Solaris

On `x86_64`, Solaris 11.4 is a Tier 1 platform, and older Solaris 11 is a Tier
2 platform.

All other Solaris from Solaris 8 and newer is an obsolete Tier 3 platform. All
Solaris versions on SPARC are obsolete Tier 3 platforms.

### IBM AIX

AIX is a Tier 1 platform for versions of AIX that are officially supported by
IBM. This currently includes:

- AIX 7.3
- AIX 7.2

The following AIX versions are Tier 2 platforms:

- AIX 7.1
- AIX 6.1

All other versions of AIX from 4.2 and newer are obsolete Tier 3 platforms.

### Microsoft Windows

For OpenAFS client components, Windows is a Tier 2 platform on `x86_64` for the
following versions (and an obsolete Tier 3 platform for 32-bit x86):

- Windows 11
- Windows 10
- Windows Server 2025
- Windows Server 2022
- Windows Server 2019
- Windows Server 2016

Windows is an obsolete Tier 3 platform for the following versions:

- Windows Server 2012
- Windows 8
- Windows Server 2008
- Windows 7
- Windows Vista
- Windows Server 2003
- Windows XP
- Windows 2000

For OpenAFS server components, Windows is a Tier 3 platform for all of the
above versions.

### BSD

For OpenAFS client components, FreeBSD is a Tier 2 platform for FreeBSD 14 and
13. Other FreeBSD 10 and newer are Tier 3.

For OpenAFS server components, FreeBSD is a Tier 3 platform for FreeBSD 14
through 10.

OpenBSD is a Tier 3 platform for OpenBSD 5.4 through 3.1.

NetBSD is a Tier 3 platform for NetBSD 7.0 through 1.5.

While there is currently no support for newer versions of these platforms,
patches to add support would be welcomed.

### Other UNIX

HP-UX is an obsolete Tier 3 platform for HP-UX 11.23 through 10.20.

SGI IRIX 6.5 is an obsolete Tier 3 platform.

## OpenAFS sysnames

An AFS sysname is a standardized identifier intended to identify a specific
combination of a CPU architecture and operating system release (though some
sysnames don't fit this definition exactly).

AFS synames are used for at least two purposes:

1. At build-time, to designate the combination of hardware architecture and
   operating system on which a particular set of binaries are targeted.
2. At run-time, as values for the magic '@sys' variable in paths, which is used
   to select files based on the client system architecture.

These two uses are actually separate, but they are often confused for each
other. In OpenAFS, the compiled-in value used for the first purpose is used as
default value for the second.

The following table lists the `sysname` identifiers that the OpenAFS build
system currently understands, and so lists all of the platforms OpenAFS can be
built for, at least partially. This list comes from the sysnames mentioned in
our `src/config/param.*.h` files.

|   OpenAFS sysname    | Description                                           |
|----------------------|-------------------------------------------------------|
|  `alpha_linux_26`    | Linux 2.6.x (Digital Alpha)                           |
|  `alpha_nbsd15`      | NetBSD 1.5 (Digital Alpha)                            |
|  `alpha_nbsd16`      | NetBSD 1.6 (Digital Alpha)                            |
|  `amd64_darwin_100`  | Apple Mac OS X 10.6 "Snow Leopard" (Intel 64)         |
|  `amd64_darwin_110`  | Apple Mac OS X 10.7 "Lion" (Intel 64)                 |
|  `amd64_darwin_120`  | Apple OS X 10.8 "Mountain Lion" (Intel 64)            |
|  `amd64_darwin_130`  | Apple OS X 10.9 "Mavericks" (Intel 64)                |
|  `amd64_darwin_140`  | Apple OS X 10.10 "Yosemite" (Intel 64)                |
|  `amd64_darwin_150`  | Apple OS X 10.11 "El Capitan" (Intel 64)              |
|  `amd64_darwin_160`  | Apple macOS 10.12 "Sierra" (Intel 64)                 |
|  `amd64_darwin_170`  | Apple macOS 10.13 "High Sierra" (Intel 64)            |
|  `amd64_darwin_180`  | Apple macOS 10.14 "Mojave" (Intel 64)                 |
|  `amd64_darwin_190`  | Apple macOS 10.15 "Catalina" (Intel 64)               |
|  `amd64_darwin_200`  | Apple macOS 11 "Big Sur" (Intel 64)                   |
|  `amd64_darwin_210`  | Apple macOS 12 "Monterey" (Intel 64)                  |
|  `amd64_darwin_220`  | Apple macOS 13 "Ventura" (Intel 64)                   |
|  `amd64_darwin_230`  | Apple macOS 14 "Sonoma" (Intel 64)                    |
|  `amd64_darwin_240`  | Apple macOS 15 "Sequoia" (Intel 64)                   |
|  `amd64_darwin_250`  | Apple macOS 26 "Tahoe" (Intel 64)                     |
|  `amd64_fbsd_100`    | FreeBSD 10.0 (amd64)                                  |
|  `amd64_fbsd_101`    | FreeBSD 10.1 (amd64)                                  |
|  `amd64_fbsd_102`    | FreeBSD 10.2 (amd64)                                  |
|  `amd64_fbsd_103`    | FreeBSD 10.3 (amd64)                                  |
|  `amd64_fbsd_104`    | FreeBSD 10.4 (amd64)                                  |
|  `amd64_fbsd_110`    | FreeBSD 11.0 (amd64)                                  |
|  `amd64_fbsd_111`    | FreeBSD 11.1 (amd64)                                  |
|  `amd64_fbsd_112`    | FreeBSD 11.2 (amd64)                                  |
|  `amd64_fbsd_113`    | FreeBSD 11.3 (amd64)                                  |
|  `amd64_fbsd_120`    | FreeBSD 12.0 (amd64)                                  |
|  `amd64_fbsd_121`    | FreeBSD 12.1 (amd64)                                  |
|  `amd64_fbsd_122`    | FreeBSD 12.2 (amd64)                                  |
|  `amd64_fbsd_123`    | FreeBSD 12.3 (amd64)                                  |
|  `amd64_fbsd_130`    | FreeBSD 13.0 (amd64)                                  |
|  `amd64_fbsd_131`    | FreeBSD 13.1 (amd64)                                  |
|  `amd64_fbsd_140`    | FreeBSD 14.0 (amd64)                                  |
|  `amd64_fbsd_141`    | FreeBSD 14.1 (amd64)                                  |
|  `amd64_linux26`     | Linux 2.6+ (amd64)                                    |
|  `amd64_nbsd20`      | NetBSD 2.0 (amd64)                                    |
|  `amd64_nbsd30`      | NetBSD 3.0 (amd64)                                    |
|  `amd64_nbsd40`      | NetBSD 4.0 (amd64)                                    |
|  `amd64_nbsd50`      | NetBSD 5.0 (amd64)                                    |
|  `amd64_nbsd60`      | NetBSD 6.0 (amd64)                                    |
|  `amd64_nbsd70`      | NetBSD 7.0 (amd64)                                    |
|  `amd64_obsd36`      | OpenBSD 3.6 (amd64)                                   |
|  `amd64_obsd37`      | OpenBSD 3.7 (amd64)                                   |
|  `amd64_obsd38`      | OpenBSD 3.8 (amd64)                                   |
|  `amd64_obsd39`      | OpenBSD 3.9 (amd64)                                   |
|  `amd64_obsd40`      | OpenBSD 4.0 (amd64)                                   |
|  `amd64_obsd41`      | OpenBSD 4.1 (amd64)                                   |
|  `amd64_obsd42`      | OpenBSD 4.2 (amd64)                                   |
|  `amd64_obsd43`      | OpenBSD 4.3 (amd64)                                   |
|  `amd64_obsd44`      | OpenBSD 4.4 (amd64)                                   |
|  `amd64_obsd45`      | OpenBSD 4.5 (amd64)                                   |
|  `amd64_obsd46`      | OpenBSD 4.6 (amd64)                                   |
|  `amd64_obsd47`      | OpenBSD 4.7 (amd64)                                   |
|  `amd64_obsd48`      | OpenBSD 4.8 (amd64)                                   |
|  `amd64_obsd49`      | OpenBSD 4.9 (amd64)                                   |
|  `amd64_obsd50`      | OpenBSD 5.0 (amd64)                                   |
|  `amd64_obsd51`      | OpenBSD 5.1 (amd64)                                   |
|  `amd64_obsd52`      | OpenBSD 5.2 (amd64)                                   |
|  `amd64_obsd53`      | OpenBSD 5.3 (amd64)                                   |
|  `amd64_obsd54`      | OpenBSD 5.4 (amd64)                                   |
|  `arm64_linux26`     | Linux 2.6+ (arm64)                                    |
|  `arm_darwin_100`    | Apple Mac OS X 10.6 "Snow Leopard" (Apple iPhone)     |
|  `arm_darwin_200`    | Apple macOS 11 "Big Sur" (Apple Silicon)              |
|  `arm_darwin_210`    | Apple macOS 12 "Monterey" (Apple Silicon)             |
|  `arm_darwin_220`    | Apple macOS 13 "Ventura" (Apple Silicon)              |
|  `arm_darwin_230`    | Apple macOS 14 "Sonoma" (Apple Silicon)               |
|  `arm_darwin_240`    | Apple macOS 15 "Sequoia" (Apple Silicon)              |
|  `arm_darwin_250`    | Apple macOS 26 "Tahoe" (Apple Silicon)                |
|  `arm_linux26`       | Linux 2.6+ (32-bit ARM)                               |
|  `hp_ux102`          | HP/UX 10.20 (32-bit PA-RISC)                          |
|  `hp_ux110`          | HP/UX 11.00 (64-bit PA-RISC)                          |
|  `hp_ux1123`         | HP/UX 11.23/11i v2 (64-bit PA-RISC)                   |
|  `hp_ux11i`          | HP/UX 11.11/11i v1 (64-bit PA-RISC)                   |
|  `i386_fbsd_100`     | FreeBSD 10.0 (i386)                                   |
|  `i386_fbsd_101`     | FreeBSD 10.1 (i386)                                   |
|  `i386_fbsd_102`     | FreeBSD 10.2 (i386)                                   |
|  `i386_fbsd_103`     | FreeBSD 10.3 (i386)                                   |
|  `i386_fbsd_104`     | FreeBSD 10.4 (i386)                                   |
|  `i386_fbsd_110`     | FreeBSD 11.0 (i386)                                   |
|  `i386_fbsd_111`     | FreeBSD 11.1 (i386)                                   |
|  `i386_fbsd_112`     | FreeBSD 11.2 (i386)                                   |
|  `i386_fbsd_113`     | FreeBSD 11.3 (i386)                                   |
|  `i386_fbsd_120`     | FreeBSD 12.0 (i386)                                   |
|  `i386_fbsd_121`     | FreeBSD 12.1 (i386)                                   |
|  `i386_fbsd_122`     | FreeBSD 12.2 (i386)                                   |
|  `i386_fbsd_123`     | FreeBSD 12.3 (i386)                                   |
|  `i386_linux26`      | Linux 2.6+ (i386)                                     |
|  `i386_nbsd15`       | NetBSD 1.5 (i386)                                     |
|  `i386_nbsd16`       | NetBSD 1.6 (i386)                                     |
|  `i386_nbsd20`       | NetBSD 2.0 (i386)                                     |
|  `i386_nbsd21`       | NetBSD 2.1 (i386)                                     |
|  `i386_nbsd30`       | NetBSD 3.0 (i386)                                     |
|  `i386_nbsd40`       | NetBSD 4.0 (i386)                                     |
|  `i386_nbsd50`       | NetBSD 5.0 (i386)                                     |
|  `i386_nbsd60`       | NetBSD 6.0 (i386)                                     |
|  `i386_nbsd70`       | NetBSD 7.0 (i386)                                     |
|  `i386_obsd31`       | OpenBSD 3.1 (i386)                                    |
|  `i386_obsd32`       | OpenBSD 3.2 (i386)                                    |
|  `i386_obsd33`       | OpenBSD 3.3 (i386)                                    |
|  `i386_obsd34`       | OpenBSD 3.4 (i386)                                    |
|  `i386_obsd35`       | OpenBSD 3.5 (i386)                                    |
|  `i386_obsd36`       | OpenBSD 3.6 (i386)                                    |
|  `i386_obsd37`       | OpenBSD 3.7 (i386)                                    |
|  `i386_obsd38`       | OpenBSD 3.8 (i386)                                    |
|  `i386_obsd39`       | OpenBSD 3.9 (i386)                                    |
|  `i386_obsd40`       | OpenBSD 4.0 (i386)                                    |
|  `i386_obsd41`       | OpenBSD 4.1 (i386)                                    |
|  `i386_obsd42`       | OpenBSD 4.2 (i386)                                    |
|  `i386_obsd43`       | OpenBSD 4.3 (i386)                                    |
|  `i386_obsd44`       | OpenBSD 4.4 (i386)                                    |
|  `i386_obsd45`       | OpenBSD 4.5 (i386)                                    |
|  `i386_obsd46`       | OpenBSD 4.6 (i386)                                    |
|  `i386_obsd47`       | OpenBSD 4.7 (i386)                                    |
|  `i386_obsd48`       | OpenBSD 4.8 (i386)                                    |
|  `i386_obsd49`       | OpenBSD 4.9 (i386)                                    |
|  `i386_obsd50`       | OpenBSD 5.0 (i386)                                    |
|  `i386_obsd51`       | OpenBSD 5.1 (i386)                                    |
|  `i386_obsd52`       | OpenBSD 5.2 (i386)                                    |
|  `i386_obsd53`       | OpenBSD 5.3 (i386)                                    |
|  `i386_obsd54`       | OpenBSD 5.4 (i386)                                    |
|  `i386_umlinux26`    | User-mode Linux 2.6 (i386)                            |
|  `ia64_hpux1122`     | HP-UX 11.22 (Itanium)                                 |
|  `ia64_hpux1123`     | HP-UX 11.23 (Itanium)                                 |
|  `ia64_linux26`      | Linux 2.6+ (Itanium)                                  |
|  `macppc_nbsd16`     | NetBSD 1.6 (Mac PowerPC)                              |
|  `macppc_nbsd20`     | NetBSD 2.0 (Mac PowerPC)                              |
|  `ppc64_darwin_100`  | Apple macOS 10.6 "Snow Leopard" (PowerPC 64)          |
|  `ppc64le_linux26`   | Linux 2.6+ (ppc64le)                                  |
|  `ppc64_linux26`     | Linux 2.6+ (ppc64)                                    |
|  `ppc_darwin_100`    | Apple Mac OS X 10.6 "Snow Leopard" (PowerPC 32)       |
|  `ppc_darwin_70`     | Apple Mac OS X 10.3 "Panther" (PowerPC 32)            |
|  `ppc_darwin_80`     | Apple Mac OS X 10.4 "Tiger" (PowerPC 32)              |
|  `ppc_darwin_90`     | Apple Mac OS X 10.5 "Leopard" (PowerPC 32)            |
|  `ppc_linux26`       | Linux 2.6+ (ppc)                                      |
|  `rs_aix42`          | IBM AIX 4.2                                           |
|  `rs_aix51`          | IBM AIX 5.1                                           |
|  `rs_aix52`          | IBM AIX 5.2                                           |
|  `rs_aix53`          | IBM AIX 5.3                                           |
|  `rs_aix61`          | IBM AIX 6.1                                           |
|  `rs_aix71`          | IBM AIX 7.1                                           |
|  `rs_aix72`          | IBM AIX 7.2                                           |
|  `rs_aix73`          | IBM AIX 7.3                                           |
|  `s390_linux26`      | Linux 2.6+ (s390)                                     |
|  `s390x_linux26`     | Linux 2.6+ (s390x)                                    |
|  `sgi_65`            | SGI IRIX 6.5                                          |
|  `sparc64_linux26`   | Linux 2.6+ (SPARC64)                                  |
|  `sparc_linux26`     | Linux 2.6+ (SPARC)                                    |
|  `sun4x_510`         | Solaris 10 (SPARC)                                    |
|  `sun4x_511`         | Solaris 11 (SPARC)                                    |
|  `sun4x_58`          | Solaris 8 (SPARC)                                     |
|  `sun4x_59`          | Solaris 9 (SPARC)                                     |
|  `sunx86_510`        | Solaris 10 (x86)                                      |
|  `sunx86_511`        | Solaris 11 (x86)                                      |
|  `sunx86_58`         | Solaris 8 (x86)                                       |
|  `sunx86_59`         | Solaris 9 (x86)                                       |
|  `x86_darwin_100`    | Apple Mac OS X 10.6 "Snow Leopard" (Intel 32)         |
|  `x86_darwin_110`    | Apple Mac OS X 10.7 "Lion" (Intel 32)                 |
|  `x86_darwin_120`    | Apple OS X 10.8 "Mountain Lion" (Intel 32)            |
|  `x86_darwin_130`    | Apple OS X 10.9 "Mavericks" (Intel 32)                |
|  `x86_darwin_140`    | Apple OS X 10.10 "Yosemite" (Intel 32)                |
|  `x86_darwin_150`    | Apple OS X 10.11 "El Capitan" (Intel 32)              |
|  `x86_darwin_160`    | Apple macOS 10.12 "Sierra" (Intel 32)                 |
|  `x86_darwin_170`    | Apple macOS 10.13 "High Sierra" (Intel 32)            |
|  `x86_darwin_180`    | Apple macOS 10.14 "Mojave" (Intel 32)                 |
|  `x86_darwin_190`    | Apple macOS 10.15 "Catalina" (Intel 32)               |
|  `x86_darwin_80`     | Apple Mac OS X 10.4 "Tiger" (Intel 32)                |
|  `x86_darwin_90`     | Apple Mac OS X 10.5 "Leopard" (Intel 32/64)           |
