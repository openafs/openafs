#!/bin/sh
#
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html
#
# This file (software) may also be used separately from OpenAFS
# under other licenses, contact the author(s) listed below for details.
#
# This software copyright 2011, 2012 Troy Benjegerdes <hozer@hozed.org>

. ../tap/libtap.sh


plan 1

if [ ! -f "$TOPDIR/src/afsd/afsd.fuse" ] ; then
    skip_all
fi

grep -e "^>" conf/CellServDB  | cut -d " " -f 1 | cut -b2- | sort > cells.tmp

(./try-fuse.sh > fuse-log 2>&1 ) &

fusepid=$!

sleep 1
ls mntdir | sort > ls.tmp
ok "ls dynroot" diff cells.tmp ls.tmp

rm cells.tmp ls.tmp

kill $fusepid

wait $fusepid
