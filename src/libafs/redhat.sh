#!/bin/sh
# This is a bridge script until we take care of tightly linking Linux inode 
# internals to AFS vnode internals

IBYTES=""
SETATTR=""

grep i_bytes /lib/modules/$1/build/include/linux/fs.h > /dev/null
if [ $? = 0 ]; then
    IBYTES="-DSTRUCT_INODE_HAS_I_BYTES=1"
fi
grep "extern int inode_setattr" /lib/modules/$1/build/include/linux/fs.h > /dev/null
if [ $? = 0 ]; then
    SETATTR="-DINODE_SETATTR_NOT_VOID=1"
fi

/bin/rm $2
echo "KDEFINES = ${IBYTES} ${SETATTR}" > $2
exit 0
