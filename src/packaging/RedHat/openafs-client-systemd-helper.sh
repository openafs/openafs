#!/bin/bash
#
# systemd helper script for openafs-client.service. This is only intended to be
# called from the openafs-client.service unit file.

set -e

[ -f /etc/sysconfig/openafs ] && . /etc/sysconfig/openafs

case $1 in
    ExecStartPre)
	if fs sysname >/dev/null 2>&1 ; then
	    echo AFS client appears to be running -- not starting
	    exit 1
	fi
	sed -n 'w/usr/vice/etc/CellServDB' /usr/vice/etc/CellServDB.local /usr/vice/etc/CellServDB.dist
	chmod 0644 /usr/vice/etc/CellServDB
	exec /sbin/modprobe openafs
	;;

    ExecStart)
	exec /usr/vice/etc/afsd $AFSD_ARGS
	;;

    ExecStop)
	/bin/umount /afs || true
	/usr/vice/etc/afsd -shutdown || true
	exec /sbin/rmmod openafs
	;;
esac

echo "Usage: $0 {ExecStartPre|ExecStart|ExecStop}" >&2
exit 1
