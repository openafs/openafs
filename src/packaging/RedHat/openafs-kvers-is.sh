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
  echo "$2" | /bin/sed -e 's/^.*[0-9L]\([^0-9L]*\)/\1/'
  exit 0
fi

if [ "$1" = "kvers" ] ; then
  #logger "parsing type from $2"
  echo "$2" | /bin/sed -e 's/^\(.*[0-9L]\)[^0-9L]*$/\1/'
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
