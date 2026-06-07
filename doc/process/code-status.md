# OpenAFS Code Maintenance Status

## Introduction

This document tracks the status of features and systems within the OpenAFS
project. The purpose is to provide a clear understanding of which features are
actively maintained and which are considered obsolete. This policy helps to
focus development resources.

Code that is not actively maintained falls into one of the following
categories:

**Experimental**

Experimental features are incomplete or unstable, and are not suitable for
production use. They are included in the codebase for testing and development
purposes. These features may change in backward-incompatible ways or be removed
entirely in a future release.

**Legacy**

Legacy code is unmaintained and not actively in use. This category includes
code that is kept for historical or backward-compatibility reasons, even if it
is non-functional on modern platforms, or has been entirely superseded.
Developers should avoid this code. Patches will only be considered to address
critical issues, such as security issues, critical bugs, and minor changes to
fix build issues introduced by new build tools versions. The long-term goal is
often to deprecate and then remove the code.

**Deprecated**

Deprecation applies to specific interfaces, functions, tools, or libraries that
have been replaced by a newer, recommended alternative. Deprecated code may
be removed in a future major release. Developers should migrate existing code
away from deprecated interfaces and use the new alternatives for any new code.
Deprecated code should be indicated with Doxygen comments.

## Experimental

The following areas of code are considered experimental.

**rxgk**

rxgk is an Rx security class that is based on GSS and can use modern
cryptography, which is intended to replace the older fcrypt-based rxkad
security class. The implementation of rxgk in OpenAFS is being actively
developed, but is not yet complete.

**FUSE**

A FUSE-based client exists in `src/afsd/afsd_fuse.c` (which builds the binary
`afsd.fuse`). But this uses `libuafs` and the high-level `fuse.h` interface,
both of which operate on entire full paths to files at a time, and isn't suited
to production use for serious filesystem activity.

This FUSE client is not well-tested and isn't used very much. The hope is that
this client can be improved someday by converting it to use the
`fuse_lowlevel.h` interface instead, and define FUSE as a `libafs` platform
instead of using `libuafs`. For example, FUSE-specific code would live in
`src/afs/FUSE`, alongside other `libafs` platforms like `LINUX`.

**Disconnected Mode**

The client cache manager includes code for a "disconnected mode" (managed by
the command `fs discon`), which would allow the client to continue to function
when disconnected from the network.  This feature is not usable in its current
state. It is retained in the codebase in the hope that it may be completed in
the future.

**Cache Bypass**

The client cache manager can be configured to bypass the on-disk cache for
certain files with the `fs bypassthreshold` command, but the feature is not
fully tested, and is only implemented at all on Linux. The feature is retained
in the codebase in the hope that it may be fully tested and production-ready in
the future.

## Legacy

**Legacy platform support**

See platform-support.md for the list of legacy platforms.

**Inode Fileserver Backend**

The `inode` fileserver backend is a method of accessing `/vicep` files on
fileservers that requires using the OpenAFS kernel module. It is untested
and/or unusable on any modern supported platforms. On modern platforms, the
inode fileserver backend has been superseded by `namei` fileserver backend.
The `namei` backend accesses `/vicep` files via standard POSIX syscalls and does
not require using the OpenAFS kernel module.

Specifying which fileserver backend to use is done at `./configure`-time, where
`--enable-namei-fileserver` is the default for all modern platforms, but the
inode backend can be forced with `--disable-namei-fileserver` on some
platforms.

The OpenAFS `vfsck` tool is only used for inode fileservers, so is also legacy.

**Non-Demand Attach File Server (non-DAFS)**

Modern deployments use the Demand Attach File Server (DAFS). The Non-DAFS
versions are considered to be legacy.

Several OpenAFS server components are built twice, as DAFS and non-DAFS
variants. For example, the fileserver process is built with DAFS as
`dafileserver` in `src/dviced` and without DAFS as `fileserver` in `src/viced`.

**AFS user account management tool (uss)**

The subsystem is based on `kauth` which makes it obsolete in its current
state.

**OpenAFS Backup system**

The OpenAFS Backup servers and clients currently use deprecated LWP threading
and need to be ported to use pthreads. The backup system is implemented in
several components in `src/bu*`, such as `src/bubasics` and `src/butc`.

**Tivoli storage manager support**

Tivoli storage manager integration with the OpenAFS Backup system (located in
`src/tsm41`) is currently not maintained.

**libuafs and UKERNEL**

libuafs is a library for accessing AFS files like a normal client, but purely
from userspace; it provides POSIX-like APIs like `uafs_open()`, `uafs_read()`,
etc., defined in `src/afs/UKERNEL/afs_usrops.h`. libuafs is built in
`src/libuafs` mostly from code in `src/afs` (the same code as the `libafs`
kernel module), but with the `UKERNEL` symbol defined.

Originally, this library and other similar libraries were used to make Java
libraries and webserver extensions, to access AFS files from inside a Java
application or webserver process without a kernel client. Those components have
been removed from OpenAFS, and the only remaining users of libuafs in the
tree are the FUSE client and the `afsload` perl program (through the libuafs
SWIG perl bindings).

The libuafs API is rather awkward to use for most use cases; it provides
interfaces that look like POSIX functions, but are slightly different, with its
own emulation of integer file descriptors. The only benefit to this design
seems to be creating a library that can intercept POSIX calls (via
`LD_PRELOAD`) to allow another process to read AFS files without going through
a real client. Such a library was never implemented, and implementing
filesystem access via something like FUSE sounds like a better way to
accomplish that.

New code should probably not use libuafs, and the existing users of libuafs
will probably go away or migrate away from libuafs. libuafs also contains SWIG
bindings to use libuafs from Perl (module AFS::ukernel), which are also
considered legacy and are just as cumbersome to use.

**afsload**

`afsload` is a perl program in `src/libafs/afsload` that was used to simulate
large numbers of scriptable AFS clients for testing purposes, using MPI and the
perl SWIG bindings for libuafs.

This general approach in retrospect is not the best way to simulate clients;
using libuafs to have a "real cache manager" adds unnecessary awkwardness and
overhead. Future work to test client load should probably instead just focus on
generating the relevant network activity, and could possibly use existing
non-OpenAFS frameworks for load-testing network protocols.

**DCE/DFS Translator**

DCE/DFS was a distributed network filesystem protocol that was originally
intended as the successor to AFS, but is no longer used. Some OpenAFS
components have some code to act as a translator between DFS and AFS
environments, but all of it is untested and has probably not worked for quite
some time. Patches to DFS translator code will probably not be accepted, unless
to remove or disable the relevant code.

Some code also exists to interpret DFS Access Control Lists (ACLs), in addition
to AFS ACLs, but this is also considered legacy and will be removed at some
point.

Note that DCE/DFS is not to be confused with Microsoft DFS, which is completely
unrelated software.

**NFS Translator**

OpenAFS contains some code to allow NFS clients to access AFS files, acting as
a translator (or "xlator") between the two protocols. This feature is no longer
really used and is not implemented for modern Linux, and is treated as a legacy
feature. This feature may seem unnecessary, since an NFS server can just export
an AFS directory itself, but using the builtin translator allowed for some
extra features like managing authenticated access and sysnames.

The commands `fs exportafs` and `knfs` are used for managing the NFS
translator, and so are also legacy.

The `rmtsys` protocol (`src/sys/rmtsys.xg`) is related; this allows clients
using NFS via a translator to issue AFS pioctls if the AFSSERVER variable is
set.

OpenAFS also contains code for a Linux kernel module called `afspag`, which was
used with the NFS translator to manage AFS credentials and sysnames.

## Deprecated

The general policy for deprecation is to identify a clear replacement and provide
a migration path. Deprecated code is a candidate for removal in a future major
release.

Specific functions should be marked as deprecated directly in the source code
comments using Doxygen tags such as `@deprecated`.

Some general areas which are deprecated:

**LWP threading**

The OpenAFS LWP threading code is being removed from the tree. New code should
use standard pthreads.  Old code using LWP threads should convert to using
pthreads when possible.  Fixes to LWP threads are not likely to be accepted.

Some OpenAFS components are built twice, as pthreaded and LWP variants. For
example, the `bosserver` process is built with pthreads in `src/tbozo` ("t" for
"threaded"), and with LWP in `src/bozo`. The utility `src/config/lwptool` is
used to more easily build pthreaded and LWP variants of source files with one
command.

**libafsutil (src/util)**

New common utility functions should be located in the common `opr` library, and
libafsutil functionality should be moved to opr whenever possible. opr can be
used in userspace and kernel space and should have no non-system dependencies.

**test scripts (src/tests)**

New tests should be added to the top-level `tests` directory. The old tests in
`src/tests` are not maintained.  Any useful tools should be migrated to the
`src/tools` hierarchy.

**Transarc-style paths**

Transarc-style paths are deprecated. Tools and packaging should use modern FHS
paths when practical. Symlinks should be used for compatibility.

"Transarc-style paths" refer to paths like `/usr/afs` and `/usr/vice`, as
opposed to FHS paths like `/usr/bin` and `/var/lib`. Transarc-style paths can
be enabled during `./configure` with `--enable-transarc-paths`.

The `dest` target in makefiles is considered deprecated. Please use the
`install` target instead.

**Kerberos v4 Support (kauth)**

The built-in support for the Kerberos v4-based kaserver authentication protocol
(aka `kauth`, or `kas`) is obsolete and has been completely superseded by
Kerberos 5.  Modern deployments must use a Kerberos v5 implementation.

Some `src/` components related to kauth support include:

- `kauth` Obsolete Kerberos-4 server (`kaserver`) and related programs
- `pam`   Pluggable authentication modules (PAM) for kauth authentication

**Server components on Microsoft Windows**

The code to run OpenAFS servers on Microsoft Windows is deprecated.  We are not
aware of any deployments of the OpenAFS servers on Microsoft Windows. Sites
running OpenAFS servers are expected to do so on Unix-like platforms.
