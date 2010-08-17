/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Interface to the volume operations used by the AFS user
 *	account facility.
 */

#ifndef _USS_VOL_H_
#define _USS_VOL_H_ 1

/*
 * ------------------------ Exported functions  -----------------------
 */
extern afs_int32 uss_vol_GetServer(char *a_name);
    /*
     * Summary:
     *    Given the string name of a desired host, find its address.
     *
     * Args:
     *    a_name : String name of desired host.
     *
     * Returns:
     *    Host address in network byte order.
     */

extern afs_int32 uss_vol_GetPartitionID(char *a_name);
    /*
     * Summary:
     *    Get partition id from a name.
     *
     * Args:
     *    a_name : Name of the partition ID.
     *
     * Returns:
     *    Numeric partition name, or -1 on failure.
     */

extern afs_int32 uss_vol_CreateVol(char *a_volname, char *a_server,
				   char *a_partition, char *a_quota,
				   char *a_mpoint, char *a_owner,
				   char *a_acl);
    /*
     * Summary:
     *    Create a volume, set its disk quota, and mount it at the
     *    given place.  Also, set the mountpoint's ACL.
     *
     * Args:
     *    char *a_volname   : Volume name to mount.
     *    char *a_server    : FileServer housing the volume
     *    char *a_partition : Partition housing the volume
     *    char *a_quota     : Initial quota
     *    char *a_mpoint    : Mountpoint to assign it
     *    char *a_owner     : Name of mountpoint's owner
     *    char *a_acl       : ACL for mountpoint.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem in the routine itself, or
     *    Other error code if problem occurred in lower-level call.
     */

extern afs_int32 uss_vol_DeleteVol(char *a_volName, afs_int32 a_volID,
				   char *a_servName, afs_int32 a_servID,
				   char *a_partName, afs_int32 a_partID);
    /*
     * Summary:
     *    Delete the given volume.
     *
     * Args:
     *    char *a_volName  : Name of the volume to delete.
     *    afs_int32 a_volID     : Numerical volume ID.
     *    char *a_servName : Name of the server hosting the volume.
     *    afs_int32 a_servID    : Numerical server ID.
     *    char *a_partName : Name of the home server partition.
     *    afs_int32 a_volID     : Numerical partition ID.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem in the routine itself, or
     *    Other error code if problem occurred in lower-level call.
     */

extern afs_int32 uss_vol_GetVolInfoFromMountPoint(char *a_mountpoint);
    /*
     * Summary:
     *    Given a mountpoint, pull out the name of the volume mounted
     *    there, along with the name of the FileServer and partition
     *    hosting it, putting them all in common locations.
     *
     * Args:
     *    char *a_mountpoint : Name of the mountpoint.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem in the routine itself, or
     *    Other error code if problem occurred in lower-level call.
     */

#if 0
extern afs_int32 uss_vol_DeleteMountPoint();
    /*
     * Summary:
     *    Given a mountpoint, nuke it.
     *
     * Args:
     *    char *a_mountpoint : Name of the mountpoint.
     *
     * Returns:
     *    0 if everything went well,
     *    1 if there was a problem in the routine itself, or
     *    Other error code if problem occurred in lower-level call.
     */
#endif
#endif /* _USS_VOL_H_ */
