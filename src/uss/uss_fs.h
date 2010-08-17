/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *	Interface to the AFS system operations exported by the
 *	Cache Manager.
 */

#ifndef _USS_FS_H_
#define _USS_FS_H_ 1

/*
 * --------------------- Exported definitions ---------------------
 */
#define USS_FS_MAX_SIZE		2048


/*
 * ---------------------- Exported variables ----------------------
 */
extern char *uss_fs_InBuff;	/*Cache Manager input buff */
extern char *uss_fs_OutBuff;	/*Cache Manager output buff */


/*
 * ------------------------ Exported functions  -----------------------
 */
extern afs_int32 uss_fs_GetACL(char *a_dirPath, char *a_aclBuff,
       			       afs_int32 a_aclBuffBytes);
    /*
     * Summary:
     *    Given the pathname for a directory, return its ACL.
     *
     * Args:
     *    char *a_dirPath       : Directory pathname.
     *    char *a_aclBuff       : Ptr to the buffer in which to put
     *                            the directory's ACL.
     *    afs_int32 a_aclBuffBytes      : Size of the above.
     *
     * Returns:
     *    0 if everything went well,
     *    -1 otherwise, with errno set to the error.
     */

extern afs_int32 uss_fs_SetACL(char *a_dirPath, char *a_aclBuff,
			       afs_int32 a_aclBuffBytes);
    /*
     * Summary:
     *    Set the ACL on the specified directory.
     *
     * Args:
     *    char *a_dirpath       : Directory pathname.
     *    char *a_aclBuff       : Ptr to the buffer from which to get
     *                            the directory's ACL.
     *    afs_int32 a_aclBuffBytes      : Size of the above.
     *
     * Returns:
     *    0 if everything went well,
     *    -1 otherwise, with errno set to the error.
     */

extern afs_int32 uss_fs_GetVolStat(char *a_mountpoint, char *a_volStatBuff,
				   afs_int32 a_volStatBuffBytes);
    /*
     * Summary:
     *    Given the pathname of an AFS mountpoint, find out what you
     *    can about the volume mounted there.
     *
     * Args:
     *    char *a_mountpoint      : Mountpoint pathname.
     *    char *a_volStatBuff     : Buffer to hold the status.
     *    afs_int32 a_volStatBuffBytes : Length of above.
     *
     * Returns:
     *    0 if everything went well,
     *    -1 otherwise, with errno set to the error.
     */

extern afs_int32 uss_fs_SetVolStat(char *a_mountpoint, char *a_volStatBuff,
		                   afs_int32 a_volStatBuffBytes);
    /*
     * Summary:
     *    Given the pathname of an AFS mountpoint, set the status info
     *    for the volume mounted there.
     *
     * Args:
     *    char *a_mountpoint      : Mountpoint pathname.
     *    char *a_volStatBuff     : Buffer holding the status.
     *    afs_int32 a_volStatBuffBytes : Length of above.
     *
     * Returns:
     *    0 if everything went well,
     *    -1 otherwise, with errno set to the error.
     */

extern afs_int32 uss_fs_CkBackups(void);
    /*
     * Summary:
     *    Make sure the CacheManager doesn't have any stale volume
     *    mappings.
     *
     * Args:
     *    None.
     *
     * Returns:
     *    0 if everything went well,
     *    -1 otherwise, with errno set to the error.
     */

extern afs_int32 uss_fs_MkMountPoint(char *a_volname, char *a_cellname,
				     afs_int32 a_rw, char *a_mountpoint);
    /*
     * Summary: *NEW*
     *    Given the name of the volume, the cell it lives in,
     *    whether we want the read/write version mounted, and
     *    the pathname for the desired mountpoint, go ahead and
     *    create it.
     *
     * Args:
     *    char *a_volname    : Name of volume to mount.
     *    char *a_cellname   : Name of cell where volume lives.
     *    afs_int32 a_rw             : Read/write mount?
     *    char *a_mountpoint : Name desired for the mountpoint.
     *
     * Returns:
     *    0 if everything went well,
     *    -1 otherwise, with errno set to the error.
     */

extern afs_int32 uss_fs_RmMountPoint(char *a_mountpoint);
    /*
     * Summary:
     *    Delete the given mountpoint.
     *
     * Args:
     *    char *a_mountpoint : Name of the mountpoint to delete.
     *
     * Returns:
     *    0 if everything went well,
     *    -1 otherwise, with errno set to the error.
     */

extern int uss_fs_UnlogToken(char *celln);

#endif /* _USS_FS_H_ */
