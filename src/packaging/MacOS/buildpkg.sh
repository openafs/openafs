#!/bin/sh

if [ -z "$1" ]; then
   echo Usage: buildpkg binary-dir
  exit 1
fi
BINDEST=$1
RESSRC=`pwd`
if [ ! -f /usr/bin/package ]; then
   echo "/usr/bin/package does not exist. Please run this script on a MacOS X system"
  echo "with the BSD subsystem installed"
  exit 1
fi
if grep -q 'set resDir = ""' /usr/bin/package ; then
   echo /usr/bin/package is buggy.
   echo remove the line \''set resDir = ""'\' from /usr/bin/package and try again
   exit 1
fi

if [ -x /usr/bin/curl ]; then
    /usr/bin/curl -f -O http://www.central.org/dl/cellservdb/CellServDB
fi

if [ ! -f CellServDB ]; then
   echo "A CellServDB file must be placed in the working directory"
   die=1
fi
FILES="ReadMe.rtf License.rtf CellServDB.list OpenAFS.info OpenAFS.post_install OpenAFS.pre_upgrade csrvdbmerge.pl"
for f in $FILES; do
   if [ ! -f $f ]; then
     echo "file missing: " $f
     die=1
   fi
done
if [ "$die" ]; then
  echo "Correct above errors; then retry"
  exit 1
fi
if [ ! -f $BINDEST/bin/translate_et ]; then
  die=1
fi
if [ ! -f $BINDEST/root.client/usr/vice/etc/afs.kext/Contents/MacOS/afs ]; then
  die=1
fi
if [ "$die" ]; then
   echo $BINDEST " is not a valid binary dir. it should be the result of"
   echo "make dest"
   exit 1
fi

PKGROOT=$RESSRC/pkgroot
PKGRES=$RESSRC/pkgres
rm -rf pkgroot pkgres
mkdir -p $PKGROOT $PKGRES

mkdir $PKGROOT/Library
chown -R root.admin $PKGROOT
chmod -R 775 $PKGROOT
mkdir $PKGROOT/Library/OpenAFS $PKGROOT/Library/OpenAFS/Tools
cd $BINDEST
pax -rw * $PKGROOT/Library/OpenAFS/Tools
cd $RESSRC
mkdir $PKGROOT/Library
mkdir $PKGROOT/Library/StartupItems 
mkdir $PKGROOT/Library/StartupItems/OpenAFS
cp $BINDEST/root.client/usr/vice/etc/afs.rc  $PKGROOT/Library/StartupItems/OpenAFS/OpenAFS
chmod a+x $PKGROOT/Library/StartupItems/OpenAFS/OpenAFS
cp $BINDEST/root.client/usr/vice/etc/StartupParameters.plist  $PKGROOT/Library/StartupItems/OpenAFS/StartupParameters.plist
chown -R root.admin $PKGROOT/Library
chmod -R o-w $PKGROOT/Library
chmod -R g+w $PKGROOT/Library
chown -R root.wheel $PKGROOT/Library/OpenAFS/Tools
chmod -R og-w $PKGROOT/Library/OpenAFS/Tools

mkdir $PKGROOT/private $PKGROOT/private/var $PKGROOT/private/var/db
mkdir $PKGROOT/private/var/db/openafs $PKGROOT/private/var/db/openafs/cache
mkdir $PKGROOT/private/var/db/openafs/etc $PKGROOT/private/var/db/openafs/etc/config
cp $RESSRC/CellServDB $PKGROOT/private/var/db/openafs/etc/CellServDB.master
echo andrew.cmu.edu > $PKGROOT/private/var/db/openafs/etc/ThisCell.sample
echo /Network/afs:/var/db/openafs/cache:30000 > $PKGROOT/private/var/db/openafs/etc/cacheinfo.sample
#echo '-stat 2000 -dcache 800 -daemons 3 -volumes 70 -rootvol root.afs.local' > $PKGROOT/private/var/db/openafs/etc/config/afsd.options.sample

strip -X -S $PKGROOT/Library/OpenAFS/Tools/root.client/usr/vice/etc/afs.kext/Contents/MacOS/afs

cp -RP $PKGROOT/Library/OpenAFS/Tools/root.client/usr/vice/etc/afs.kext $PKGROOT/private/var/db/openafs/etc

chown -R root.wheel $PKGROOT/private
chmod -R og-w $PKGROOT/private
chmod  og-rx $PKGROOT/private/var/db/openafs/cache

mkdir $PKGROOT/usr $PKGROOT/usr/bin $PKGROOT/usr/sbin

BINLIST="fs klog klog.krb kpasswd pagsh pagsh.krb pts sys tokens tokens.krb unlog unlog.krb"

# Should these be linked into /usr too?
OTHER_BINLIST="bos cmdebug rxgen translate_et udebug xstat_cm_test xstat_fs_test"
OTHER_ETCLIST="vos rxdebug"

for f in $BINLIST; do
   ln -s ../../Library/OpenAFS/Tools/bin/$f $PKGROOT/usr/bin/$f
done

ln -s ../../Library/OpenAFS/Tools/root.client/usr/vice/etc/afsd $PKGROOT/usr/sbin/afsd

chown -R root.wheel $PKGROOT/usr
chmod -R og-w $PKGROOT/usr

cp License.rtf ReadMe.rtf OpenAFS.post_install OpenAFS.pre_upgrade $PKGRES
cp OpenAFS.post_install $PKGRES/OpenAFS.post_upgrade
chmod a+x $PKGRES/OpenAFS.post_install $PKGRES/OpenAFS.post_upgrade $PKGRES/OpenAFS.pre_upgrade
cp csrvdbmerge.pl $PKGRES
chmod a+x $PKGRES/csrvdbmerge.pl
cp CellServDB.list $PKGRES
chown -R root.wheel $PKGRES
rm -rf OpenAFS.pkg
echo /usr/bin/package $PKGROOT OpenAFS.info -r $PKGRES
/usr/bin/package $PKGROOT OpenAFS.info -r $PKGRES
#old versions of package didn't handle this properly
if [ ! -r OpenAFS.pkg/Contents ]; then
	mkdir OpenAFS.pkg/Contents OpenAFS.pkg/Contents/Resources
	mv OpenAFS.pkg/OpenAFS.* OpenAFS.pkg/Contents/Resources
	mv OpenAFS.pkg/*.rtf OpenAFS.pkg/Contents/Resources
	mv OpenAFS.pkg/csrvdbmerge.pl OpenAFS.pkg/Contents/Resources
	mv OpenAFS.pkg/CellServDB* OpenAFS.pkg/Contents/Resources
fi

rm -rf pkgroot pkgres
# Unfortunately, sudo sets $USER to root, so I can't chown the 
#.pkg dir back to myself
#chown -R $USER OpenAFS.pkg
