#!/bin/sh

# stop_afs.sh
# 
#
# Created by Claudio Bisegni on 24/06/07.
# Copyright 2007 INFN. All rights reserved.

VICEETC=$1/etc
AFSD=$2
echo "Stopping AFS"

echo "Unmounting /afs"
umount -f /afs 2>&1 > /dev/console

echo "Shutting down afsd processes"
$AFSD -shutdown 2>&1 > /dev/console

echo "Unloading AFS kernel extensions"
kextunload $VICEETC/afs.kext 2>&1 > /dev/console

