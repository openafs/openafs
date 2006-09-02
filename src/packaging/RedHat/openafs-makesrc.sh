#!/bin/sh
#
# openafs-makesrc -- convert a 2.6 'kernel' package into something
#   you can install into /usr/src/kernels
#
# Written by:  Derek Atkins <warlord@MIT.EDU>
#
# $Revision: 1.1.2.1 $

kerndir=/usr/src/kernels

[ -d $kerndir ] || mkdir -p -m 0755 $kerndir
umask 022
while [ -n "$1" ] ; do
  rpm=$1
  name=`rpm -qp $rpm`
  vers=`echo $name | sed -e 's/kernel-[^0-9]*\([0-9].*\)$/\1/'`
  smp=`echo $name | sed -e 's/kernel-\([^0-9]*\)[0-9].*$/\1/' -e s/-//`
  arch=`echo $rpm | sed 's/.*\.\([^\.]*\)\.rpm$/\1/'`

  kd=$kerndir/$vers$smp-$arch
  if [ ! -d $kd ] ; then
    echo "converting `basename $rpm` to $kd"
    rpm2cpio $rpm | ( cd $kerndir ; cpio --quiet -imd \*lib/modules/\*/build/\* )
    mv $kerndir/lib/modules/*/build $kd
    chmod 755 $kd
    rmdir $kerndir/lib/modules/*
    rmdir $kerndir/lib/modules
    rmdir $kerndir/lib
  else
    echo "$kd already exists.  Ignoring."
  fi
  shift
done
