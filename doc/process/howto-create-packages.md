# Packaging Guide

## Building Packages for RHEL-like distributions

This guide shows how to build OpenAFS RPM packages for Red Hat Enterprise
Linux, CentOS, and Fedora, AlmaLinux, and Rocky Linux, using the contributed
RPM spec file provided with the OpenAFS source code.

When building packages from a Git checkout, be sure to always build in a clean
directory. In particular be sure the 'packages' directory is absent or empty
before building RPM files. Also, be sure there are no uncommitted source code
changes. This method will only package code which has been committed, any
uncommitted changes will not be packaged.

The following steps assume you are building packages for the current master
branch.  See the [OpenAFS wiki](https://wiki.openafs.org) for information on
building packages for release versions.

### Setup

Upgrade your kernel to the most recent version to be sure a matching
kernel development package can be installed.

    $ sudo yum update kernel
    $ sudo reboot  # if updated

After rebooting (if needed), install the kernel headers for your current kernel
version.

    $ sudo yum install "kernel-devel-uname-r == $(uname -r)"

Install the build requirements:

    $ sudo yum install \
        autoconf \
        automake \
        bison \
        bzip2 \
        elfutils-devel \
        flex \
        fuse-devel \
        gcc \
        glibc-devel \
        krb5-devel \
        libtool \
        make \
        ncurses-devel \
        openssl-devel \
        pam-devel \
        perl \
        perl-devel \
        perl-ExtUtils-Embed \
        rpm-build \
        swig

### Build RPM Packages

Checkout the commit to be built:

    $ git checkout <ref>

Ensure the current directory is clean:

    $ git clean -d -x -q -f

Generate the Makefile:

    $ ./regen.sh -q
    $ ./configure --disable-kernel-module

Build the packages:

    $ make rpm

Source and binary RPM files will be placed under the `packages` directory.


## Debian Packages

How to create OpenAFS Debian packages with patches.  The Debian packaging
files are maintained in the Debian **salsa.debian.org** git repository.

### Setup

Ensure your system is up to date and install build requirements:

    $ sudo apt-get update
    $ sudo apt-get install build-essential fakeroot devscripts lintian
    $ sudo apt-get build-dep openafs

## Build debs

Download the OpenAFS source and packaging from the **salsa** repository.

    $ mkdir Debian
    $ cd Debian
    $ git clone https://salsa.debian.org/debian/openafs.git
    $ cd openafs
    $ git checkout <branch>  # e.g., origin/buster

Build the **debs** with the command:

    $ dpkg-buildpackage -us -uc -b -rfakeroot

The packages will be placed in the parent directory.

To also build the source package, generate the original tarball and omit
the -b option.

    $ debian/rules get-orig-source
    $ mv openafs-<version>tar.gz ..
    $ dpkg-buildpackage -us -uc -rfakeroot
