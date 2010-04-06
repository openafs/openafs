#!/bin/sh

if [ -z "$1" ]; then
    echo Usage: universal topdir
    exit 1
fi

BINDEST=`cd $1 && pwd`
CURDIR=`pwd`
majorvers=`uname -r | sed 's/\..*//'`

DIRLIST="root.server/usr/afs/bin bin etc lib root.client/usr/vice/etc/afsd root.client/usr/vice/etc/afs.kext/Contents/MacOS/afs"
mkdir $CURDIR/u_darwin_80

(cd $BINDEST/ppc_darwin_80; tar cf - .)|(cd $CURDIR/u_darwin_80; tar xf -)
(cd $BINDEST/x86_darwin_80; tar cf - .)|(cd $CURDIR/u_darwin_80; tar xf -)

for d in $DIRLIST; do
    for f in `cd $CURDIR/u_darwin_80/dest && find $d -type f -print`; do
	/bin/rm -f $CURDIR/u_darwin_80/dest/$f
	lipo $BINDEST/ppc_darwin_80/dest/$f $BINDEST/x86_darwin_80/dest/$f -create -output $CURDIR/u_darwin_80/dest/$f
    done
done
