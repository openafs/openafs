#!/bin/sh
#
# build all of openafs for all the various kernels installed on this system.
#
# Written by:  Derek Atkins <warlord@MIT.EDU>
#
# $Revision: 1.1.2.2 $
#

# Define where the Specfile is located.
specdir=/usr/src/redhat/SPECS

# Define the rpmbuild options you want to supply.
buildopts=

############################################################################
#  Figure out the release version
[ -f /etc/redhat-release ] && rhrel=`cat /etc/redhat-release`
[ -f /etc/SuSE-release ] && rhrel=`head -1  /etc/SuSE-release`

if [ `echo $rhrel | grep -c 'Fedora Core'` = 1 ] ; then
  ostype='fc'
  osrel=`echo $rhrel | sed -e 's/^.*release \([^ ]*\).*$/\1/' -e 's/\.//g'`
elif [ `echo $rhrel | grep -c 'Red Hat Enterprise Linux'` = 1 ] ; then
  ostype='rhel'
  excludearch=i586
  osrel=`echo $rhrel | sed -e 's/^.*release \([^ ]*\).*$/\1/' -e 's/\.//g'`
elif [ `echo $rhrel | grep -c 'Red Hat Linux'` = 1 ] ; then
  ostype='rh'
  osrel=`echo $rhrel | sed -e 's/^.*release \([^ ]*\).*$/\1/' -e 's/\.//g'`
elif [ `echo $rhrel | grep -c 'SUSE LINUX'` = 1 ] ; then
  ostype='suse'
  specdir=/usr/src/packages/SPECS 
  osrel=`grep VERSION /etc/SuSE-release|awk '{print $3}'`
else
  echo "Unknown Linux Release: $rhrel"
  exit 1
fi
osvers="$ostype$osrel"

############################################################################
# Now figure out the kernel version.  We assume that the running
# kernel version is "close enough" to tell us whether it's a
# 2.4 or 2.6 kernel.
kvers=`uname -r`
kbase=/usr/src/linux-

case $kvers in
  2.4.*)
    kv=2.4.
    ;;
  2.6.*)
    [ -d /usr/src/kernels ] && kbase=/usr/src/kernels/
    kv=2.6.
    ;;
  *)
    echo "I don't know how to build for kernel $kvers"
    exit 1
    ;;
esac
############################################################################
# Now build the packages and all the kernel modules

echo "Building OpenAFS for $osvers"
rpmbuild -ba $buildopts --define "osvers $osvers" $specdir/openafs.spec || \
  exit 1

kernels=`ls -d ${kbase}${kv}*`

for kerndir in $kernels ; do
  # Ignore symlinks
  if [ ! -h $kerndir ] ; then
    vers=`echo $kerndir | sed -e "s^${kbase}^^g" -e 's/-smp/smp/g' \
          -e 's/-hugemem/hugemem/g' -e 's/-largesmp/largesmp/g'`
    if [ $kv = "2.4." ] ; then
      kvers=$vers
      case `uname -m` in
	  i386|i486|i586|i686|athlon) archlist="i586 i686 athlon" ;;
          *) archlist=`uname -m` ;;
      esac
      for a in $excludearch ; do
        archlist=`echo $archlist | sed -e s/$a//`
      done
      kend="''"
    else
      karch=`echo $vers | sed 's/.*-//'`
      kvers=`echo $vers | sed s/-$karch//`
      archlist=$karch
      kend=-$karch
    fi

    for arch in $archlist ; do
      echo "Building for $kerndir, $kvers, $arch"
        rpmbuild -bb $buildopts \
	  --define "osvers $osvers" \
	  --define "kernvers $kvers" \
	  --define "ksrcdir $kerndir" \
	  --define "build_modules 1" \
	  --target=$arch \
	  $specdir/openafs.spec || exit 1
    done
  fi
done
