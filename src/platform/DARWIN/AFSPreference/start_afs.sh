#!/bin/sh

# start_afs.sh
# 
#
# Created by Claudio Bisegni on 24/06/07.
# Copyright 2007 INFN. All rights reserved.
#
# Portions Copyright (c) 2003 Apple Computer, Inc.
#
# Updated to match standard service scripts
# Phil Holland <hollandp@umich.edu>

. /etc/rc.common

CheckForNetwork

VICEETC=$1/etc
AFSD=$2

CONFIG=$VICEETC/config
AFSDOPT=$CONFIG/afsd.options
PACKAGE=$CONFIG/package.options
VERBOSE=
if [ -x /usr/sbin/kextstat ]; then KMODSTAT=/usr/sbin/kextstat; fi
if [ -x /usr/sbin/kmodstat ]; then KMODSTAT=/usr/sbin/kmodstat; fi


if [ -f $CONFIG/afs.conf ]; then
    . $CONFIG/afs.conf
fi

# Check this file second so that if users have altered the file, it will
# override the default options
if [ -f $AFSDOPT ]; then
    OPTIONS=`cat $AFSDOPT`
fi
	
	echo "Starting OpenAFS"

    if [ -z "$OPTIONS" ] || [ "$OPTIONS" = "AUTOMATIC" ] ; then
		AFSD_OPTIONS="$VERBOSE"
    else
		AFSD_OPTIONS="$OPTIONS $VERBOSE"
    fi

    if [ "${NETWORKUP}" = "-NO-" ]; then
        echo $AFSD_OPTIONS | grep -e '-dynroot' || exit
    fi

# Need the commands ps, awk, kill, sleep
    PATH=${PATH}${PATH:+:}/sbin:/bin:/usr/bin

    if [ -d $VICEETC/afs.kext ]; then
	echo "Loading AFS kernel extensions"
	kextload $VICEETC/afs.kext
    else
	echo "$VICEETC/afs.kext does not exist. Skipping AFS startup."
	exit 1
    fi

    if $KMODSTAT | perl -e 'exit not grep /openafs/, <>' ; then
	:
    else
	echo  "AFS kernel extensions failed to initialize. Skipping AFS startup."
	exit 1
    fi

#
# Check that all of the client configuration files exist
#

    for file in $AFSD $VICEETC/cacheinfo \
	$VICEETC/ThisCell $VICEETC/CellServDB
      do
      if [ ! -f ${file} ]; then
	  echo "${file} does not exist. Not starting AFS client."
	  exit 1
      fi
    done

#
# Check that the root directory for AFS (/afs) 
# and the cache directory (/usr/vice/cache) both exist
#

    for dir in `awk -F: '{print $1, $2}' $VICEETC/cacheinfo`
      do
      if [ ! -d ${dir} ]; then
	  echo "${dir} does not exist. Not starting AFS client."
	  exit 2
      fi
    done

    echo "Starting afsd"
    $AFSD $AFSD_OPTIONS

#
# From /var/db/openafs/etc/config/afs.conf, call a post-init function or
# command if it's been defined
#
    $AFS_POST_INIT

#
# Call afssettings (if it exists) to set customizable parameters
#
    if [ -x $CONFIG/afssettings ]; then
	sleep 2
	$CONFIG/afssettings
    fi

