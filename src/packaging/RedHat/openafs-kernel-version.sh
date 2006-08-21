#!/bin/sh
#
# investigate the name of the kernel
#
# $Revision$

if [ "x$1" = "x" ] ; then
    kernvers=`uname -r`
    if [ ! -d /lib/modules/$kernvers/build/include ] ; then
        kernvers=`/bin/ls /lib/modules/*/build/include/linux/version.h | \
        cut -d/ -f4 | \
        sort -u | \
        tail -1`
    fi
else
    kernvers=$1
fi

if [ -z "$kernvers" ]; then echo "unable to determine kernel version" >&2; exit 1; fi

# strip "kernel-" off of the front
if expr "$kernvers" : "kernel-" >&/dev/null
then
    kernvers=`expr "$kernvers" : 'kernel-\(.*\)'`
fi

# Strip kernel config mnemonic off 2.4 kernels.
case $kernvers in
  2.4.*)
    # strip kernel config mnemonic off of the tail
    case "$kernvers" in
      *smp)
         kernvers=`expr "$kernvers" : '\(.*\)smp'`
         ;;
      *bigmem)
         kernvers=`expr "$kernvers" : '\(.*\)bigmem'`
         ;;
      *hugemem)
         kernvers=`expr "$kernvers" : '\(.*\)hugemem'`
         ;;
      *enterprise)
         kernvers=`expr "$kernvers" : '\(.*\)enterprise'`
         ;;
    esac
    ;;
esac

echo $kernvers
exit 0
