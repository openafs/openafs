#!/bin/sh -x
#
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html
#
# This file (software) may also be used separately from OpenAFS
# under other licenses, contact the author(s) listed below for details.
#
# copyright 2011, 2012 Troy Benjegerdes <hozer@hozed.org>

TESTDIR=$PWD
TOPDIR=$PWD/../..

export LD_LIBRARY_PATH=$TOPDIR/lib

#in case there's a stale mount here
fusermount -u $TESTDIR/mntdir

$TOPDIR/src/afsd/afsd.fuse -dynroot -fakestat -d -confdir $TESTDIR/conf -cachedir $TESTDIR/vcache -mountdir $TESTDIR/mntdir


fusermount -u $TESTDIR/mntdir
