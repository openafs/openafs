#!/bin/sh
# Portions Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.

if [ -z "$1" ]; then
   echo Usage: buildpkg binary-dir
  exit 1
fi
BINDEST=$1
RESSRC=`pwd`
majorvers=`uname -r | sed 's/\..*//'`
if [ $majorvers -ge 7 ]; then
    SEP=:
    package=/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker
    if [ ! -x $package ]; then
       echo "PackageMaker does not exist. Please run this script on a MacOS X system"
      echo "with the DeveloperTools package installed"
      exit 1
    fi
else
    SEP=.
    package=/usr/bin/package
    if [ ! -f $package ]; then
       echo "$package does not exist. Please run this script on a MacOS X system"
      echo "with the BSD subsystem installed"
      exit 1
    fi
    if grep -q 'set resDir = ""' $package ; then
       echo $package is buggy.
       echo remove the line \''set resDir = ""'\' from $package and try again
       exit 1
    fi
fi

if [ -x /usr/bin/curl ]; then
#    /usr/bin/curl -f -O http://www.central.org/dl/cellservdb/CellServDB
    /usr/bin/curl -f -O http://dl.central.org/dl/cellservdb/CellServDB
fi

if [ ! -f CellServDB ]; then
   echo "A CellServDB file must be placed in the working directory"
   die=1
fi
FILES="ReadMe.rtf License.rtf CellServDB.list OpenAFS.info OpenAFS.post_install OpenAFS.pre_upgrade csrvdbmerge.pl 2.0.txt"
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
chown -R root${SEP}admin $PKGROOT
chmod -R 775 $PKGROOT
mkdir $PKGROOT/Library/OpenAFS $PKGROOT/Library/OpenAFS/Tools
cd $BINDEST
pax -rw * $PKGROOT/Library/OpenAFS/Tools
cd $RESSRC
mkdir $PKGROOT/Library/StartupItems 
mkdir $PKGROOT/Library/StartupItems/OpenAFS
cp $BINDEST/root.client/usr/vice/etc/afs.rc  $PKGROOT/Library/StartupItems/OpenAFS/OpenAFS
chmod a+x $PKGROOT/Library/StartupItems/OpenAFS/OpenAFS
cp $BINDEST/root.client/usr/vice/etc/StartupParameters.plist  $PKGROOT/Library/StartupItems/OpenAFS/StartupParameters.plist
chown -R root${SEP}admin $PKGROOT/Library
chmod -R o-w $PKGROOT/Library
chmod -R g+w $PKGROOT/Library
chown -R root${SEP}wheel $PKGROOT/Library/StartupItems
chmod -R og-w $PKGROOT/Library/StartupItems
chown -R root${SEP}wheel $PKGROOT/Library/OpenAFS/Tools
chmod -R og-w $PKGROOT/Library/OpenAFS/Tools

mkdir $PKGROOT/private $PKGROOT/private/var $PKGROOT/private/var/db
mkdir $PKGROOT/private/var/db/openafs $PKGROOT/private/var/db/openafs/cache
mkdir $PKGROOT/private/var/db/openafs/etc $PKGROOT/private/var/db/openafs/etc/config
cp $RESSRC/CellServDB $PKGROOT/private/var/db/openafs/etc/CellServDB.master
echo openafs.org > $PKGROOT/private/var/db/openafs/etc/ThisCell.sample
if [ $majorvers -ge 7 ]; then
    echo /afs:/var/db/openafs/cache:30000 > $PKGROOT/private/var/db/openafs/etc/cacheinfo.sample
    make AFSINCLUDE=$BINDEST/include
    cp afssettings $PKGROOT/private/var/db/openafs/etc/config
    cp settings.plist $PKGROOT/private/var/db/openafs/etc/config/settings.plist.orig
    make clean
else
    echo /Network/afs:/var/db/openafs/cache:30000 > $PKGROOT/private/var/db/openafs/etc/cacheinfo.sample
fi
echo '-stat 2000 -dcache 800 -daemons 3 -volumes 70 -dynroot -fakestat-all' > $PKGROOT/private/var/db/openafs/etc/config/afsd.options.sample

strip -X -S $PKGROOT/Library/OpenAFS/Tools/root.client/usr/vice/etc/afs.kext/Contents/MacOS/afs

cp -RP $PKGROOT/Library/OpenAFS/Tools/root.client/usr/vice/etc/afs.kext $PKGROOT/private/var/db/openafs/etc

chown -R root${SEP}wheel $PKGROOT/private
chmod -R og-w $PKGROOT/private
chmod  og-rx $PKGROOT/private/var/db/openafs/cache

mkdir $PKGROOT/usr $PKGROOT/usr/bin $PKGROOT/usr/sbin

BINLIST="fs klog klog.krb pagsh pagsh.krb pts sys tokens tokens.krb unlog unlog.krb aklog"

# Should these be linked into /usr too?
OTHER_BINLIST="bos cmdebug rxgen translate_et udebug xstat_cm_test xstat_fs_test"
OTHER_ETCLIST="vos rxdebug"

for f in $BINLIST; do
   ln -s ../../Library/OpenAFS/Tools/bin/$f $PKGROOT/usr/bin/$f
done
ln -s ../../Library/OpenAFS/Tools/bin/kpasswd $PKGROOT/usr/bin/kpasswd.afs

ln -s ../../Library/OpenAFS/Tools/root.client/usr/vice/etc/afsd $PKGROOT/usr/sbin/afsd

chown -R root${SEP}wheel $PKGROOT/usr
chmod -R og-w $PKGROOT/usr

if [ $majorvers -ge 7 ]; then
    cp OpenAFS.post_install $PKGRES/postinstall
    cp OpenAFS.pre_upgrade $PKGRES/preupgrade
    cp OpenAFS.post_install $PKGRES/postupgrade
    cp InstallationCheck $PKGRES
    mkdir $PKGRES/English.lproj
    cp InstallationCheck $PKGRES/English.lproj
    chmod a+x $PKGRES/postinstall $PKGRES/postupgrade $PKGRES/preupgrade $PKGRES/InstallationCheck
else
    cp OpenAFS.post_install OpenAFS.pre_upgrade $PKGRES
    cp OpenAFS.post_install $PKGRES/OpenAFS.post_upgrade
    chmod a+x $PKGRES/OpenAFS.post_install $PKGRES/OpenAFS.post_upgrade $PKGRES/OpenAFS.pre_upgrade
fi
cp License.rtf ReadMe.rtf $PKGRES
cp csrvdbmerge.pl $PKGRES
chmod a+x $PKGRES/csrvdbmerge.pl
cp CellServDB.list $PKGRES
chown -R root${SEP}wheel $PKGRES
rm -rf OpenAFS.pkg
if [ $majorvers -ge 7 ]; then
    echo $package -build -p $RESSRC/OpenAFS.pkg -f $PKGROOT -r $PKGRES \
	-i OpenAFS.Info.plist -d OpenAFS.Description.plist
    $package -build -p $RESSRC/OpenAFS.pkg -f $PKGROOT -r $PKGRES \
	-i OpenAFS.Info.plist -d OpenAFS.Description.plist
else
    echo $package $PKGROOT OpenAFS.info -r $PKGRES
    $package $PKGROOT OpenAFS.info -r $PKGRES
    #old versions of package didn't handle this properly
    if [ ! -r OpenAFS.pkg/Contents ]; then
	    mkdir OpenAFS.pkg/Contents OpenAFS.pkg/Contents/Resources
	    mv OpenAFS.pkg/OpenAFS.* OpenAFS.pkg/Contents/Resources
	    mv OpenAFS.pkg/*.rtf OpenAFS.pkg/Contents/Resources
	    mv OpenAFS.pkg/csrvdbmerge.pl OpenAFS.pkg/Contents/Resources
	    mv OpenAFS.pkg/CellServDB* OpenAFS.pkg/Contents/Resources
    fi
fi

rm -rf pkgroot pkgres
mkdir dmg
mv OpenAFS.pkg dmg
hdiutil create -srcfolder dmg -volname OpenAFS -anyowners OpenAFS.dmg
rm -rf dmg
# Unfortunately, sudo sets $USER to root, so I can't chown the 
#.pkg dir back to myself
#chown -R $USER OpenAFS.pkg
