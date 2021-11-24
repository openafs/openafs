Guide to the OpenAFS source tree
================================

This is a guide to the various directories in the OpenAFS source code tree.
Some directories contain source code for components of the OpenAFS system,
while other directories are used by the build system to build libraries, kernel
modules, and program variations. See the `Makefile` at the top of the project
tree for build order and inter-dependencies.

RPC interface definitions (IDL)
-------------------------------

The following directory contains interface definitions (IDL) files for the
OpenAFS file server and cache manager.

| directory | description |
| --------- | ----------- |
| fsint     | File server and cache manager RPC interface definitions |

Cache Managers
--------------

The following directories contain the source code of OpenAFS cache manager and
related programs.

| directory | description |
| --------- | ----------- |
| afs       | The OpenAFS cache manager as a kernel-module for unix-like systems |
| afsd      | The `afsd` user-space program to start the cache manager on unix-like systems |
| aklog     | The Kerberos 5 login programs: `aklog`, `asetkey`, `akeyconvert` |
| sys       | NFS translator local and remote system call interfaces |
| venus     | The `fs` program and other cache manager related utilities |
| WINNT     | The OpenAFS cache manager and installer for MS Windows |
| export    | Symbol exports for AIX kernel module |

Servers
-------

The following directories contain the source code of the OpenAFS servers and
administrative command suites.

| directory | description |
| --------- | ----------- |
| bozo      | The basic overseer server and `bos` command suite |
| ptserver  | The protection server and `pts` command suite |
| update    | The update server and client for maintaining server software and configuration (deprecated) |
| viced     | The file server |
| vlserver  | The volume location server |
| volser    | The volume server and `vos` command suite |

Backup system
-------------

The following directories contain the source code of the OpenAFS backup system.

| directory | description |
| --------- | ----------- |
| bubasics  | Backup related RPC interface definitions |
| bucoord   | Backup coordinator and support library |
| budb      | Backup database server `buserver` |
| butc      | Backup tape coordinator, and `read_tape` restoration client |
| butm      | Backup tape manager library |
| `bu_utils` | Backup utilities to estimate tape capacity and file mark sizes |
| tsm41     | Tivoli storage manager support |


Rx subsystem
------------

The following directories contain the source code of the Rx subsystem. Rx
provides Remote Procedure Calls (RPC) over UDP/IP in user-space and kernel-mode,
and includes an XDR implementation to encode and decode binary data.

| directory | description |
| --------- | ----------- |
| rx        | The Rx library for kernel and user-space |
| rxdebug   | Debugging tool for rx |
| rxgen     | The `rxgen` program to generate AFS RPC stubs from IDL (i.e., `*.xg`) |
| rxgk      | Rxgk security class for rx |
| rxkad     | Kerberos security module for rx |
| rxosd     | Rx object storage device extensions IDL files |
| rxstat    | Rx statistics package |

Libraries
---------

The following directories contain the source code for the various libraries
used by OpenAFS.

| directory | description |
| --------- | ----------- |
| opr       | General purpose library for user-space and headers for kernel mode |
| audit     | The `libaudit` event auditing support library |
| auth      | The `libauth` library to manage key and cell configuration files |
| cmd       | The `libcmd` library to parse command line arguments |
| comerr    | The `com_err` library to provide error table lookup |
| dir       | Modules to lookup, add, and remove entries in directory objects |
| gtx       | A curses-based graphics library for terminal-based programs |
| kopenafs  | The `libkopenafs` library for setting up stand-alone setpag() function for Heimdal/KTH libkafs |
| libacl    | The `libacl` library for access control lists |
| libadmin  | The `libadmin` administration API |
| libafscp  | The `libafscp` library; client operations without a cache manager |
| libafsrpc | The `libafsrpc` library; basic rx functionality for programs which do not require authentication |
| libafsauthent | The `libafsauthent` library; for AFS-aware utilities |
| procmgmt  | Process management library |
| ubik      | Ubik replicated database library |
| usd       | User-space device input/output library |
| util      | Utility library (deprecated in favor of opr) |
| vol       | Volume library for managing volumes on-disk |

Utilities
---------

The following directories contain source code for utilities and libraries for
OpenAFS administrators.

| directory | description |
| --------- | ----------- |
| afsmonitor | Terminal-based file server and cache manager monitoring tool |
| fsprobe   | File Server probe library and program |
| scout     | Terminal-based file server monitoring tool |
| tools     | Contributed configuration and maintenance tools |
| xstat     | Command line tools and libraries for gathering file server and cache manager statistics |

Other
-----

| directory | description |
| --------- | ----------- |
| external  | Contains imported code which is maintained in other open source projects |
| platform  | Code for platform-specific programs and build specs. |


Build system
------------

The following directories are build directories or contain components
for the build system.

| directory | description |
| --------- | ----------- |
| cf        | OpenAFS specific M4 macros for `autoconf`  |
| config    | Build system OS-specific configuration |
| crypto    | `libhcrypto` library build directory |
| dviced    | Demand Attach File Server (DAFS) build directory |
| dvolser   | Demand Attach Volume Server build directory  |
| finale    | Final stage build directory; includes `translate_et` which translates error codes to descriptive messages |
| libafs    | OpenAFS kernel modules build directory |
| libuafs   | OpenAFS user-space cache manager build directory |
| packaging | Contributed packaging files for various platforms |
| roken     | Build directory for `libroken`, a set of os-independent functions|
| tbudb     | Pthreaded Backup Server (`budb`) build directory |
| tbutc     | Pthreaded Backup Tape Coordinator (`butc`) build directory |
| tptserver | Pthreaded Protection Server (`ptserver`) build directory |
| tsalvaged | Pthreaded Salvage Server build directory; Used by Demand Attach File Server |
| tubik     | Pthreaded ubik build directory |
| tvlserver | Pthreaded Volume Location `vlserver` build directory |
| tvolser   | Pthreaded Volume Server build directory |


Not Maintained
--------------

The following directories contain code of obsolete components which are no
longer actively maintained.

| directory | description |
| --------- | ----------- |
| kauth     | Obsolete Kerberos-4 server (`kaserver`) and related programs |
| log       | Obsolete programs to show and forget Kerberos-4 tokens |
| lwp       | Lightweight user-level non-preemptive cooperative threading library |
| pam       | Pluggable authentication modules (PAM) for kauth authentication |
| tests     | Legacy test scripts; new unit tests should be added to the top level `tests` directory |
| uss       | Tool for managing users; limited to kauth |
| vfsck     | OpenAFS specific `fsck` for obsolete inode-based file server partitions |
