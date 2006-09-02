# $Id: buildpkg.sh,v 1.3.2.1 2006/06/23 14:21:12 rees Exp $

# This doesn't actually build a package any more

SRC=../../../../..
umask 022

rm -rf usr
mkdir -p usr/vice/bin usr/vice/etc usr/vice/cache
chmod 700 usr/vice/cache

ln -s \
  $SRC/comerr/compile_et \
  $SRC/venus/fs \
  $SRC/kauth/kas \
  $SRC/kauth/klog \
  $SRC/sys/pagsh \
  $SRC/ptserver/pts \
  $SRC/log/tokens \
  $SRC/log/unlog \
  $SRC/volser/vos \
  $SRC/bozo/bos \
  usr/vice/bin
ln -s $SRC/pinstall/pinstall usr/vice/bin/install

ln -s \
  $SRC/afsd/afsd \
  $SRC/libafs/MODLOAD/libafs.o \
  ../../../postinstall \
  usr/vice/etc
ln -s $SRC/afsd/afs.rc.obsd usr/vice/etc/rc.securelevel.afs

echo '/afs:/usr/vice/cache:96000' >usr/vice/etc/cacheinfo

tar chfvz openafs-client.tgz usr/vice

#pkg_create -v -h -f packinglist -c -OpenAFS -d desc -p / -s $PWD openafs-client
