#!/bin/sh
# This is a bridge script until we take care of tightly linking Linux inode 
# internals to AFS vnode internals

IBYTES=""
SETATTR=""

if [ -e $1/include/linux/fs.h ] ; then
grep i_bytes $1/include/linux/fs.h > /dev/null
if [ $? = 0 ]; then
IBYTES="-DSTRUCT_INODE_HAS_I_BYTES=1"
fi

grep "extern int inode_setattr" $1/include/linux/fs.h > /dev/null
if [ $? = 0 ]; then
SETATTR="-DINODE_SETATTR_NOT_VOID=1"
fi

if [ -e $2 ] ; then
/bin/rm $2
fi
echo "KDEFINES = ${IBYTES} ${SETATTR}" > $2
fi
exit 0
