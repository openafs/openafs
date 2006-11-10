#!/bin/sh
# Portions Copyright (c) 2003, 2006 Apple Computer, Inc.  All rights reserved.

if [ -z "$1" ]; then
    echo Usage: buildpkg binary-dir
    echo '  or'
    echo 'Usage: buildpkg [-firstpass] binary-dir'
    echo '            (customize pkgroot)'
    echo '       buildpkg [-secondpass]'
    exit 1
fi

firstpass=yes
secondpass=yes
if [ "$1" = "-firstpass" ]; then
    secondpass=no
    shift
elif [ "$1" = "-secondpass" ]; then
    firstpass=no
    shift
fi

BINDEST=`cd $1 && pwd`
CURDIR=`pwd`
RESSRC=`dirname $0`
RESSRC=`cd $RESSRC && pwd`
majorvers=`uname -r | sed 's/\..*//'`

PKGROOT=$CURDIR/pkgroot
PKGRES=$CURDIR/pkgres
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

if [ $firstpass = yes ]; then
    if [ -x /usr/bin/curl ]; then
	/usr/bin/curl -f -O http://dl.central.org/dl/cellservdb/CellServDB
    fi

    if [ ! -f CellServDB ]; then
       echo "A CellServDB file must be placed in the working directory"
       die=1
    else
       if grep -q 'GCO Public CellServDB' CellServDB ; then
         touch CellServDB
       else
          echo "A proper CellServDB file must be placed in the working directory"
          die=1
       fi
    fi
    FILES="ReadMe.rtf License.rtf CellServDB.list OpenAFS.info OpenAFS.post_install OpenAFS.pre_upgrade csrvdbmerge.pl 2.0.txt"
    for f in $FILES; do
       if [ ! -f $RESSRC/$f ]; then
	 echo "file missing: " $RESSRC/$f
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

    rm -rf $PKGROOT
    mkdir $PKGROOT

    mkdir -p $PKGROOT/Library
    chown root${SEP}admin $PKGROOT
    chmod 775 $PKGROOT $PKGROOT/Library
    mkdir -p $PKGROOT/Library/OpenAFS/Tools
    (cd $BINDEST && pax -rw * $PKGROOT/Library/OpenAFS/Tools)
    cd $RESSRC
    mkdir -p $PKGROOT/Library/StartupItems/OpenAFS
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

    mkdir -p $PKGROOT/private/var/db/openafs/cache
    mkdir -p $PKGROOT/private/var/db/openafs/etc/config
    cp $CURDIR/CellServDB $PKGROOT/private/var/db/openafs/etc/CellServDB.master
    echo grand.central.org > $PKGROOT/private/var/db/openafs/etc/ThisCell.sample
    if [ $majorvers -ge 7 ]; then
	echo /afs:/var/db/openafs/cache:30000 > $PKGROOT/private/var/db/openafs/etc/cacheinfo.sample
        cp -RP $PKGROOT/Library/OpenAFS/Tools/etc/afssettings $PKGROOT/private/var/db/openafs/etc/config
	cp settings.plist $PKGROOT/private/var/db/openafs/etc/config/settings.plist.orig
    else
	echo /Network/afs:/var/db/openafs/cache:30000 > $PKGROOT/private/var/db/openafs/etc/cacheinfo.sample
    fi
    echo '-memcache -afsdb -stat 2000 -dcache 800 -daemons 3 -volumes 70 -dynroot -fakestat-all' > $PKGROOT/private/var/db/openafs/etc/config/afsd.options.sample

    strip -X -S $PKGROOT/Library/OpenAFS/Tools/root.client/usr/vice/etc/afs.kext/Contents/MacOS/afs

    cp -RP $PKGROOT/Library/OpenAFS/Tools/root.client/usr/vice/etc/afs.kext $PKGROOT/private/var/db/openafs/etc
    cp -RP $PKGROOT/Library/OpenAFS/Tools/root.client/usr/vice/etc/C $PKGROOT/private/var/db/openafs/etc

    chown -R root${SEP}wheel $PKGROOT/private
    chmod -R og-w $PKGROOT/private
    chmod  og-rx $PKGROOT/private/var/db/openafs/cache

    mkdir -p $PKGROOT/usr/bin $PKGROOT/usr/sbin

    BINLIST="fs klog klog.krb pagsh pagsh.krb pts sys tokens tokens.krb unlog unlog.krb aklog"
    ETCLIST="vos"

# Should these be linked into /usr too?
    OTHER_BINLIST="bos cmdebug rxgen translate_et udebug xstat_cm_test xstat_fs_test"
    OTHER_ETCLIST="rxdebug"

    for f in $BINLIST; do
       ln -s ../../Library/OpenAFS/Tools/bin/$f $PKGROOT/usr/bin/$f
    done
    for f in $ETCLIST; do
       ln -s ../../Library/OpenAFS/Tools/etc/$f $PKGROOT/usr/sbin/$f
    done

    ln -s ../../Library/OpenAFS/Tools/bin/kpasswd $PKGROOT/usr/bin/kpasswd.afs

    ln -s ../../Library/OpenAFS/Tools/root.client/usr/vice/etc/afsd $PKGROOT/usr/sbin/afsd

#    mkdir -p $PKGROOT/Library/Kerberos\ Plug-Ins
#    ln -s ../../Library/OpenAFS/Tools/root.client/Library/Kerberos\ Plug-Ins/aklog.loginLogout $PKGROOT/Library/Kerberos\ Plug-Ins/

    chown -R root${SEP}wheel $PKGROOT/usr
    chmod -R og-w $PKGROOT/usr
fi

if [ $secondpass = yes ]; then
    rm -rf $PKGRES
    mkdir $PKGRES

    cd $RESSRC
    if [ $majorvers -ge 7 ]; then
	cp OpenAFS.post_install $PKGRES/postinstall
	cp OpenAFS.pre_upgrade $PKGRES/preupgrade
	cp OpenAFS.post_install $PKGRES/postupgrade
	cp background.jpg $PKGRES/background.jpg
	if [ $majorvers -ge 8 ]; then
	    cp InstallationCheck.$majorvers $PKGRES/InstallationCheck
	    mkdir -p $PKGRES/English.lproj
	    cp InstallationCheck $PKGRES/English.lproj
	    chmod a+x $PKGRES/InstallationCheck
	fi
	chmod a+x $PKGRES/postinstall $PKGRES/postupgrade $PKGRES/preupgrade
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
    rm -rf $CURDIR/OpenAFS.pkg
    if [ $majorvers -ge 7 ]; then
	echo $package -build -p $CURDIR/OpenAFS.pkg -f $PKGROOT -r $PKGRES \
	    -i OpenAFS.Info.plist -d OpenAFS.Description.plist
	$package -build -p $CURDIR/OpenAFS.pkg -f $PKGROOT -r $PKGRES \
	    -i OpenAFS.Info.plist -d OpenAFS.Description.plist
    else
	echo $package $PKGROOT $RESSRC/OpenAFS.info -r $PKGRES
	(cd $CURDIR && $package $PKGROOT $RESSRC/OpenAFS.info -r $PKGRES)
	#old versions of package didn't handle this properly
	if [ ! -r $CURDIR/OpenAFS.pkg/Contents ]; then
		mkdir -p $CURDIR/OpenAFS.pkg/Contents/Resources
		mv $CURDIR/OpenAFS.pkg/OpenAFS.* $CURDIR/OpenAFS.pkg/Contents/Resources
		mv $CURDIR/OpenAFS.pkg/*.rtf $CURDIR/OpenAFS.pkg/Contents/Resources
		mv $CURDIR/OpenAFS.pkg/csrvdbmerge.pl $CURDIR/OpenAFS.pkg/Contents/Resources
		mv $CURDIR/OpenAFS.pkg/CellServDB* $CURDIR/OpenAFS.pkg/Contents/Resources
	fi
    fi

    rm -rf $PKGROOT $PKGRES
    mkdir $CURDIR/dmg
    mv $CURDIR/OpenAFS.pkg $CURDIR/dmg
    rm -rf $CURDIR/OpenAFS.dmg
    cp Uninstall $CURDIR/dmg/Uninstall.command
    cp DS_Store $CURDIR/dmg/.DS_Store
    mkdir $CURDIR/dmg/.background
    cp afslogo.jpg $CURDIR/dmg/.background
#    hdiutil create -srcfolder $CURDIR/dmg -volname OpenAFS -anyowners $CURDIR/OpenAFS.dmg
    hdiutil makehybrid -hfs -hfs-volume-name OpenAFS -hfs-openfolder $CURDIR/dmg $CURDIR/dmg -o $CURDIR/TMP.dmg
    hdiutil convert -format UDZO TMP.dmg -o $CURDIR/OpenAFS.dmg
    rm $CURDIR/TMP.dmg
    rm -rf $CURDIR/dmg
    # Unfortunately, sudo sets $USER to root, so I can't chown the 
    #.pkg dir back to myself
    #chown -R $USER $CURDIR/OpenAFS.pkg
else
    echo "First pass completed.  Customize pkgroot and then run:"
    echo "    $0 -secondpass"
fi
