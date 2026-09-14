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

- [Y] Alistair Ferguson <alistair.ferguson@morganstanley.com> (Morgan Stanley)
- [?] Anders Kaseorg <andersk@mit.edu>
- [Y] Andrei Keis <Andrei.Keis@morganstanley.com> (Morgan Stanley)
- [Y] Andrew Deason <adeason@dson.org>
- [Y] Andrew Deason <adeason@sinenomine.net> (SNA)
- [?] Arno Schuring <aelschuring@hotmail.com>
- [?] Benjamin Kaduk <kaduk@mit.edu>
- [Y] Carsten Jacobi <jacobi@de.ibm.com> (IBM)
- [Y] Cesar Garcia <cesarg@ms.com> (Morgan Stanley)
- [?] Chas Williams (CONTRACTOR) <chas@cmf.nrl.navy.mil> (USN)
- [?] chas williams - CONTRACTOR <chas@cmf.nrl.navy.mil> (USN)
- [?] Chas Williams <3chas3@gmail.com>
- [?] Chas Williams <chas@cmf.nrl.navy.mil> (USN)
- [?] Chaskiel Grundman <cg2v@andrew.cmu.edu>
- [?] Chaskiel M Grundman <cg2v@andrew.cmu.edu>
- [?] Chaz Chandler <clc31@inbox.com>
- [?] Cheyenne Wills <cwills@sinenomine.net> (SNA)
- [?] Dan Hyde <drh@umich.edu>
- [Y] Daria Phoebe Brashear <shadow@andrew.cmu.edu>
- [Y] Daria Phoebe Brashear <shadow@dementia.org>
- [Y] Daria Phoebe Brashear <shadow@dementix.org>
- [Y] Daria Phoebe Brashear <shadow@gmail.com>
- [?] David Howells <bartbanter@hotmail.com>
- [?] Derek Atkins <warlord@mit.edu>
- [?] Ed Moy <emoy@apple.com> (Apple Inc.)
- [Y] Erik J. Burckart <ejburcka@us.ibm.com> (IBM)
- [?] Felix Frank <Felix.Frank@Alumni.TU-Berlin.de>
- [Y] Ganesh Chaudhari <gangovind@in.ibm.com> (IBM)
- [?] Garrett Wollman <wollman@csail.mit.edu>
- [?] Hartmut Reuter <reuter@rzg.mpg.de>
- [Y] Indira Sawant <indira.sawant@ibm.com> (IBM)
- [Y] James Peterson <jimpeter@us.ibm.com> (IBM)
- [Y] Jeff Riegel <riegel@almaden.ibm.com> (IBM)
- [?] Jeffrey Altman <jaltman@grand.central.org>
- [?] Jeffrey Altman <jaltman@mit.edu>
- [?] Jeffrey Altman <jaltman@secure-endpoints.com>
- [?] Jeffrey Altman <jaltman@your-file-system.com> (YFS)
- [?] Jeffrey Hutzelman <jhutz@cmu.edu>
- [Y] Jeremy Stribling <jstribl@us.ibm.com> (IBM)
- [?] Jim Rees <rees@umich.edu>
- [Y] Kailas Zadbuke <kailashsz@in.ibm.com> (IBM)
- [?] Kevin Coffman <kwc@citi.umich.edu>
- [Y] Laura Stentz <stentz@us.ibm.com> (IBM)
- [?] Love Hörnquist-Åstrand <lha@e.kth.se>
- [Y] Manuel Pereira <mpereira@almaden.ibm.com> (IBM)
- [Y] Manuel Pereira <mpereira@us.ibm.com> (IBM)
- [?] Marc Dionne <marc.c.dionne@gmail.com>
- [?] Marcio Barbosa <mbarbosa@sinenomine.net> (SNA)
- [?] Marcus Watts <mdw@umich.edu>
- [?] Mark Vitale <mvitale@sinenomine.net> (SNA)
- [?] Matt Benjamin <matt@linuxbox.com>
- [?] Matt Smith <matt.j.sm@gmail.com>
- [Y] Matthew A. Bacchi <mbacchi@btv.ibm.com> (IBM)
- [?] Michael Meffie <mmeffie@sinenomine.net> (SNA)
- [Y] Michael Niksch <nik@zurich.ibm.com> (IBM)
- [?] Nathan Neulinger <nneul@umr.edu>
- [Y] Omkar Sathe <somkar@in.ibm.com> (IBM)
- [Y] Paul Smeddle <paul.smeddle@morganstanley.com> (Morgan Stanley)
- [?] Rainer Schöpf <rainer.schoepf@proteosys.com>
- [?] Rainer Toebbicke <rtb@pclella.cern.ch>
- [?] Rolf Sattler <rolf@multi-os-net.de>
- [?] Russ Allbery <rra@stanford.edu> (Stanford)
- [Y] Satish Kumar <ksatish@in.ibm.com> (IBM)
- [Y] Shyh-Wei Luan <luan@almaden.ibm.com> (IBM)
- [?] Simon Wilkinson <sxw@inf.ed.ac.uk>
- [?] Simon Wilkinson <sxw@your-file-system.com> (YFS)
- [Y] Srikanth Vishwanathan <vsrikanth@in.ibm.com> (IBM)
- [Y] Sven Oehme <oehmes@de.ibm.com> (IBM)
- [Y] Ted Anderson <ota@transarc.com> (IBM)
- [Y] Todd DeSantis <atd@us.ibm.com> (IBM)
- [?] Tom Maher <tardis@ece.cmu.edu>
- [?] Walter Wong <wcw@cmu.edu>
- [Y] Yadav Yadavendra <yadayada@in.ibm.com> (IBM)
- [Y] Yadavendra Yadav <yadayada@in.ibm.com> (IBM)

Copyright Held by Other Legal Entities
======================================

The contributors above may have contributed the code on behalf of a company
that holds the copyright. This list tracks such legal entities. The contributor
list above indicates (in parentheses) if a contributor provided code for a
legal entity here.

- [?] Apple Inc.
- [Y] International Business Machines Corporation (IBM)
- [?] Leland Stanford Junior University (Stanford)
- [Y] Morgan Stanley
- [?] Sine Nomine Associates, Inc. (SNA)
- [?] United States Navy (USN)
- [?] Your File System, Inc. (YFS)
