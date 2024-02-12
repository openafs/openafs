#!/bin/bash
#
# systemd helper script for openafs-client.service. This is only intended to be
# called from the openafs-client.service unit file.

set -e

[ -f /etc/sysconfig/openafs ] && . /etc/sysconfig/openafs

case $1 in
    ExecStart)
	if fs sysname >/dev/null 2>&1 ; then
	    # If we previously tried to stop the client and failed (because
	    # e.g. /afs was in use), our unit will be deactivated but the
	    # client will keep running. So if we're starting up, but the client
	    # is currently running, do not perform the startup sequence but
	    # just return success, to let the unit activate, so stopping the
	    # unit can go through the shutdown sequence again.
	    echo AFS client appears to be running -- skipping startup
	    exit 0
	fi
	sed -n 'w/usr/vice/etc/CellServDB' /usr/vice/etc/CellServDB.local /usr/vice/etc/CellServDB.dist
	chmod 0644 /usr/vice/etc/CellServDB

	# If the kernel module is already initialized from a previous client
	# run, it must be unloaded and loaded again. So if the module is
	# currently loaded, unload it in case it was (partly) initialized.
	if lsmod | grep -wq ^openafs ; then
	    /sbin/rmmod --verbose openafs
	fi
	/sbin/modprobe --verbose openafs
	exec /usr/vice/etc/afsd $AFSD_ARGS
	;;

    ExecStop)
	if /bin/umount --verbose /afs ; then
	    exit 0
	else
	    echo "Failed to unmount /afs: $?"
	fi
	exit 1
	;;

    ExecStopPost)
	/usr/vice/etc/afsd -shutdown || true
	/sbin/rmmod --verbose openafs || true
	if lsmod | grep -wq ^openafs ; then
	    echo "Cannot unload the OpenAFS client kernel module."
	    echo "systemd will consider the openafs-client.service unit inactive, but the AFS client may still be running."
	    echo "To stop the client, stop all access to /afs, and then either:"
	    echo "stop the client manually:"
	    echo "    umount /afs"
	    echo "    rmmod openafs"
	    echo "or start and stop the openafs-client.service unit:"
	    echo "    systemctl start openafs-client.service"
	    echo "    systemctl stop openafs-client.service"
	    echo 'See "journalctl -u openafs-client.service" for details.'
	    exit 1
	fi
	exit 0
	;;
esac

echo "Usage: $0 {ExecStart|ExecStop|ExecStopPost}" >&2
exit 1
