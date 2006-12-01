#!/bin/sh
# Small helper script for parsing kernel versions and types
# $Revision$

if [ "$1" = "parsev" ] ; then
  #logger "parsing version from $2"
  echo "$2" | /bin/sed -e 's/^\([0-9]*\.[0-9]*\)\..*/\1/'
  exit 0
fi

if [ "$1" = "parset" ] ; then
  #logger "parsing type from $2"
  case "$2" in
    *smp) echo 'smp' ;;
    *hugemem) echo 'hugemem' ;;
  esac
  exit 0
fi

if [ "$1" = "kvers" ] ; then
  #logger "parsing type from $2"
  echo "$2" | /bin/sed -re 's/smp$//; s/hugemem$//;'
  exit 0
fi

if [ "$1" = "find" ] ; then
  f=`find "$2"/configs -name \*-"$3"-"$4".config`
  if [ -n "$f" ] ; then
    echo 1
    exit 1
  else
    echo 0
    exit 0
  fi
fi

# provideskernelarch ksrcdir targetcpu
if [ "$1" = "provideskernelarch" ]; then
  kernelrpm=`rpm -qf "$2"`
  if [ -n "$kernelrpm" ]; then
    if rpm -q --provides "$kernelrpm" |egrep -q "^kernel(|-devel)-$3 "; then
      echo 1
      exit 1
    fi
  fi
  echo 0
  exit 0
fi

#logger "comparing $1 to $2"
if [ "$1" = "$2" ] ; then
  #logger "yes"
  echo 1
  exit 1
else
  #logger "no"
  echo 0
  exit 0
fi
