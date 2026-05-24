Relicensing OpenAFS
===================

As of 2026, the OpenAFS project contains many files in the source tree
licensed under the IBM Public License v1.0 ("IPL"), and a few files licensed
under the Sun RPC License. OpenAFS is undergoing an effort to change the
license of these files.


What is Changing?
=================

1. All files previously available under the IBM Public License v1.0
   (SPDX: IPL-1.0) will now also be available under the GNU General
   Public License v2.0 as an alternative to the IPL (SPDX: IPL-1.0 OR
   GPL-2.0-only).

2. All files previously available under the Sun RPC License will now be
   available under the BSD 3-Clause License (SPDX: BSD-3-Clause).

For each impacted file, the license change takes effect when the license
agreement statement is changed in that file in the git tree. Some files may
have their license updated before the complete tree is updated, provided that
the necessary approvals for such files are in place at the time of relicensing.


Why Relicensing?
================

GPLv2 and the BSD 3-Clause License are much more common licenses for
free software projects to use, which generally make it easier to share
code between projects.

The use of the IPL and the Sun RPC License also causes practical
problems:

1. We want to continue to provide a kernel module for new Linux kernel
   versions, and new Linux kernel interfaces are often only usable by
   GPL-licensed kernel modules. The IPL is not GPL-compatible, and so OpenAFS
   cannot distribute a GPL-licensed kernel module, and so currently OpenAFS
   cannot use GPL-only interfaces, which makes supporting the Linux kernel
   module difficult.

2. The Sun RPC License is also not GPL-compatible, and furthermore is widely
   considered a non-free license. In addition to issues with Linux
   interfaces, this license causes issues with including OpenAFS in
   Linux distributons such as Debian.

Fully relicensing the relevant files is a large effort. This file keeps
track of copyright holders that agree or disagree to such a license change.


Which Code?
===========

All files available under the IBM Public License v1.0 have a notice near
the top of the file that mentions the IPL. For example:

```
/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
```

All files available under the Sun RPC License have a copyright notice
for Sun Microsystems near the top of the file. As of OpenAFS commit
7ba0f3b6a5, this consists of the following files:

```
src/rx/xdr.c
src/rx/xdr.h
src/rx/xdr_array.c
src/rx/xdr_arrayn.c
src/rx/xdr_float.c
src/rx/xdr_int32.c
src/rx/xdr_mem.c
src/rx/xdr_rec.c
src/rx/xdr_reference.c
src/rx/xdr_stdio.c
src/rxgen/rpc_cout.c
src/rxgen/rpc_hout.c
src/rxgen/rpc_main.c
src/rxgen/rpc_parse.c
src/rxgen/rpc_parse.h
src/rxgen/rpc_scan.c
src/rxgen/rpc_scan.h
src/rxgen/rpc_util.c
src/rxgen/rpc_util.h
```


Consent/Disapproval about What?
===============================

Whether the copyright holder agrees to relicense their contributions to OpenAFS
under the terms of the licenses in the "What is Changing?" section above.


How to Track Consent/Disapproval?
=================================

The consent/disapproval is tracked in this file for authors who
made their wish known. In the list below [Y] indicates consent while [N]
indicates disapproval. [?] indicates yet unknown data.  To express the wish,
either send an email to our mailing list <openafs-devel@openafs.org> or submit
a change to <https://gerrit.openafs.org>, adding your name to the list in this
file. Then this file will be updated to track the information. The git commit
messages in the history of this file will give details when/how an entry was
added.

The list below consists of authors as reported by git, which is not always the
copyright holder. A [Y] in this list indicates that we have sufficient consent
from the appropriate copyright holders to relicense contributions authored by
that git author.


Consent/Disapproval List
========================


Copyright Held by Other Legal Entities
======================================

The contributors above may have contributed the code on behalf of a company
that holds the copyright. This list tracks such legal entities. The contributor
list above indicates (in parentheses) if a contributor provided code for a
legal entity here.
