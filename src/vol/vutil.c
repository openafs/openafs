/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	System:		VICE-TWO
	Module:		vutil.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#include <afsconfig.h>
#include <afs/param.h>


#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <time.h>
#include <fcntl.h>
#else
#include <sys/time.h>
#include <sys/file.h>
#include <unistd.h>
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <afs/afs_assert.h>

#include <rx/xdr.h>
#include <afs/afsint.h>
#include "nfs.h"
#include <afs/errors.h>
#include "lock.h"
#include "lwp.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include <afs/afsutil.h>
#ifdef AFS_NT40_ENV
#include "ntops.h"
#include <io.h>
#endif
#include "vnode.h"
#include "volume.h"
#include "volume_inline.h"
#include "partition.h"
#include "viceinode.h"

#include "volinodes.h"
#include "vol_prototypes.h"
#include "common.h"

#ifdef	AFS_AIX_ENV
#include <sys/lockf.h>
#endif
#if defined(AFS_SUN5_ENV) || defined(AFS_NT40_ENV) || defined(AFS_LINUX20_ENV)
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef O_LARGEFILE
#define afs_open	open64
#else /* !O_LARGEFILE */
#define afs_open	open
#endif /* !O_LARGEFILE */


#ifndef AFS_NT40_ENV
# ifdef O_LARGEFILE
#  define AFS_SETLKW   F_SETLKW64
#  define AFS_SETLK    F_SETLK64
#  define afs_st_flock flock64
# else
#  define AFS_SETLKW   F_SETLKW
#  define AFS_SETLK    F_SETLK
#  define afs_st_flock flock
# endif
#endif

/* Note:  the volume creation functions herein leave the destroyMe flag in the
   volume header ON:  this means that the volumes will not be attached by the
   file server and WILL BE DESTROYED the next time a system salvage is performed */

#ifdef FSSYNC_BUILD_CLIENT
static void
RemoveInodes(struct afs_inode_info *stuff, Device dev, VolumeId parent,
             VolumeId vid)
{
    int i;
    IHandle_t *handle;

    /* This relies on the fact that IDEC only needs the device and NT only
     * needs the dev and vid to decrement volume special files.
     */
    IH_INIT(handle, dev, parent, -1);
    for (i = 0; i < MAXINODETYPE; i++) {
	Inode inode = *stuff[i].inode;
	if (VALID_INO(inode)) {
	    if (stuff[i].inodeType == VI_LINKTABLE) {
		IH_DEC(handle, inode, parent);
	    } else {
		IH_DEC(handle, inode, vid);
	    }
	}
    }
    IH_RELEASE(handle);
}

Volume *
VCreateVolume(Error * ec, char *partname, VolId volumeId, VolId parentId)
{				/* Should be the same as volumeId if there is
				 * no parent */
    Volume *retVal;
    VOL_LOCK;
    retVal = VCreateVolume_r(ec, partname, volumeId, parentId);
    VOL_UNLOCK;
    return retVal;
}

Volume *
VCreateVolume_r(Error * ec, char *partname, VolId volumeId, VolId parentId)
{				/* Should be the same as volumeId if there is
				 * no parent */
    VolumeDiskData vol;
    int i, rc;
    char headerName[VMAXPATHLEN], volumePath[VMAXPATHLEN];
    Device device;
    struct DiskPartition64 *partition;
    struct VolumeDiskHeader diskHeader;
    IHandle_t *handle;
    FdHandle_t *fdP;
    Inode nearInode AFS_UNUSED = 0;
    char *part, *name;
    struct stat st;
    afs_ino_str_t stmp;
    struct VolumeHeader tempHeader;
    struct afs_inode_info stuff[MAXINODETYPE];
# ifdef AFS_DEMAND_ATTACH_FS
    int locktype = 0;
# endif /* AFS_DEMAND_ATTACH_FS */

    init_inode_info(&tempHeader, stuff);

    *ec = 0;
    memset(&vol, 0, sizeof(vol));
    vol.id = volumeId;
    vol.parentId = parentId;
    vol.copyDate = time(0);	/* The only date which really means when this
				 * @i(instance) of this volume was created.
				 * Creation date does not mean this */

    /* Initialize handle for error case below. */
    handle = NULL;

    /* Verify that the parition is valid before writing to it. */
    if (!(partition = VGetPartition_r(partname, 0))) {
	Log("VCreateVolume: partition %s is not in service.\n", partname);
	*ec = VNOVOL;
	return NULL;
    }
#if	defined(NEARINODE_HINT)
    nearInodeHash(volumeId, nearInode);
    nearInode %= partition->f_files;
#endif
    VGetVolumePath(ec, vol.id, &part, &name);
    if (*ec == VNOVOL || !strcmp(partition->name, part)) {
	/* this case is ok */
    } else {
	/* return EXDEV if it's a clone or read-only to an alternate partition
	 * otherwise assume it's a move */
	if (vol.parentId != vol.id) {
	    Log("VCreateVolume: volume %lu for parent %lu"
		" found on %s; unable to create volume on %s.\n",
		afs_printable_uint32_lu(vol.id),
		afs_printable_uint32_lu(vol.parentId), part, partition->name);
	    *ec = EXDEV;
	    return NULL;
	}
    }
    *ec = 0;

# ifdef AFS_DEMAND_ATTACH_FS
    /* volume doesn't exist yet, but we must lock it to try to prevent something
     * else from reading it when we're e.g. half way through creating it (or
     * something tries to create the same volume at the same time) */
    locktype = VVolLockType(V_VOLUPD, 1);
    rc = VLockVolumeByIdNB(volumeId, partition, locktype);
    if (rc) {
	Log("VCreateVolume: vol %lu already locked by someone else\n",
	    afs_printable_uint32_lu(volumeId));
	*ec = VNOVOL;
	return NULL;
    }
# else /* AFS_DEMAND_ATTACH_FS */
    VLockPartition_r(partname);
# endif /* !AFS_DEMAND_ATTACH_FS */

    memset(&tempHeader, 0, sizeof(tempHeader));
    tempHeader.stamp.magic = VOLUMEHEADERMAGIC;
    tempHeader.stamp.version = VOLUMEHEADERVERSION;
    tempHeader.id = vol.id;
    tempHeader.parent = vol.parentId;
    vol.stamp.magic = VOLUMEINFOMAGIC;
    vol.stamp.version = VOLUMEINFOVERSION;
    vol.destroyMe = DESTROY_ME;
    (void)afs_snprintf(headerName, sizeof headerName, VFORMAT, afs_printable_uint32_lu(vol.id));
    (void)afs_snprintf(volumePath, sizeof volumePath, "%s" OS_DIRSEP "%s",
		       VPartitionPath(partition), headerName);
    rc = stat(volumePath, &st);
    if (rc == 0 || errno != ENOENT) {
	if (rc == 0) {
	    Log("VCreateVolume: Header file %s already exists!\n",
		volumePath);
	    *ec = VVOLEXISTS;
	} else {
	    Log("VCreateVolume: Error %d trying to stat header file %s\n",
	        errno, volumePath);
	    *ec = VNOVOL;
	}
	goto bad_noheader;
    }
    device = partition->device;

    for (i = 0; i < MAXINODETYPE; i++) {
	struct afs_inode_info *p = &stuff[i];
	if (p->obsolete)
	    continue;
#ifdef AFS_NAMEI_ENV
	*(p->inode) =
	    IH_CREATE(NULL, device, VPartitionPath(partition), nearInode,
		      (p->inodeType == VI_LINKTABLE) ? vol.parentId : vol.id,
		      INODESPECIAL, p->inodeType, vol.parentId);
	if (!(VALID_INO(*(p->inode)))) {
	    if (errno == EEXIST && (p->inodeType == VI_LINKTABLE)) {
		/* Increment the reference count instead. */
		IHandle_t *lh;
		int code;

		*(p->inode) = namei_MakeSpecIno(vol.parentId, VI_LINKTABLE);
		IH_INIT(lh, device, parentId, *(p->inode));
		fdP = IH_OPEN(lh);
		if (fdP == NULL) {
		    IH_RELEASE(lh);
		    goto bad;
		}
		code = IH_INC(lh, *(p->inode), parentId);
		FDH_REALLYCLOSE(fdP);
		IH_RELEASE(lh);
		if (code < 0)
		    goto bad;
		continue;
	    }
	}
#else
	*(p->inode) =
	    IH_CREATE(NULL, device, VPartitionPath(partition), nearInode,
		      vol.id, INODESPECIAL, p->inodeType, vol.parentId);
#endif

	if (!VALID_INO(*(p->inode))) {
	    Log("VCreateVolume:  Problem creating %s file associated with volume header %s\n", p->description, volumePath);
	  bad:
	    if (handle)
		IH_RELEASE(handle);
	    RemoveInodes(stuff, device, vol.parentId, vol.id);
	    if (!*ec) {
		*ec = VNOVOL;
	    }
	    VDestroyVolumeDiskHeader(partition, volumeId, parentId);
	  bad_noheader:
# ifdef AFS_DEMAND_ATTACH_FS
	    if (locktype) {
		VUnlockVolumeById(volumeId, partition);
	    }
# endif /* AFS_DEMAND_ATTACH_FS */
	    return NULL;
	}
	IH_INIT(handle, device, vol.parentId, *(p->inode));
	fdP = IH_OPEN(handle);
	if (fdP == NULL) {
	    Log("VCreateVolume:  Problem iopen inode %s (err=%d)\n",
		PrintInode(stmp, *(p->inode)), errno);
	    goto bad;
	}
	if (FDH_PWRITE(fdP, (char *)&p->stamp, sizeof(p->stamp), 0) !=
	    sizeof(p->stamp)) {
	    Log("VCreateVolume:  Problem writing to inode %s (err=%d)\n",
		PrintInode(stmp, *(p->inode)), errno);
	    FDH_REALLYCLOSE(fdP);
	    goto bad;
	}
	FDH_REALLYCLOSE(fdP);
	IH_RELEASE(handle);
	nearInode = *(p->inode);
    }

    IH_INIT(handle, device, vol.parentId, tempHeader.volumeInfo);
    fdP = IH_OPEN(handle);
    if (fdP == NULL) {
	Log("VCreateVolume:  Problem iopen inode %s (err=%d)\n",
	    PrintInode(stmp, tempHeader.volumeInfo), errno);
	goto bad;
    }
    if (FDH_PWRITE(fdP, (char *)&vol, sizeof(vol), 0) != sizeof(vol)) {
	Log("VCreateVolume:  Problem writing to  inode %s (err=%d)\n",
	    PrintInode(stmp, tempHeader.volumeInfo), errno);
	FDH_REALLYCLOSE(fdP);
	goto bad;
    }
    FDH_CLOSE(fdP);
    IH_RELEASE(handle);

    VolumeHeaderToDisk(&diskHeader, &tempHeader);
    rc = VCreateVolumeDiskHeader(&diskHeader, partition);
    if (rc) {
	Log("VCreateVolume: Error %d trying to write volume header for "
	    "volume %u on partition %s; volume not created\n", rc,
	    vol.id, VPartitionPath(partition));
	if (rc == EEXIST) {
	    *ec = VVOLEXISTS;
	}
	goto bad;
    }

# ifdef AFS_DEMAND_ATTACH_FS
    if (locktype) {
	VUnlockVolumeById(volumeId, partition);
    }
# endif /* AFS_DEMAND_ATTACH_FS */
    return (VAttachVolumeByName_r(ec, partname, headerName, V_SECRETLY));
}
#endif /* FSSYNC_BUILD_CLIENT */


void
AssignVolumeName(VolumeDiskData * vol, char *name, char *ext)
{
    VOL_LOCK;
    AssignVolumeName_r(vol, name, ext);
    VOL_UNLOCK;
}

void
AssignVolumeName_r(VolumeDiskData * vol, char *name, char *ext)
{
    char *dot;
    strncpy(vol->name, name, VNAMESIZE - 1);
    vol->name[VNAMESIZE - 1] = '\0';
    dot = strrchr(vol->name, '.');
    if (dot && (strcmp(dot, ".backup") == 0 || strcmp(dot, ".readonly") == 0))
	*dot = 0;
    if (ext)
	strncat(vol->name, ext, VNAMESIZE - 1 - strlen(vol->name));
}

afs_int32
CopyVolumeHeader_r(VolumeDiskData * from, VolumeDiskData * to)
{
    /* The id and parentId fields are not copied; these are inviolate--the to volume
     * is assumed to have already been created.  The id's cannot be changed once
     * creation has taken place, since they are embedded in the various inodes associated
     * with the volume.  The copydate is also inviolate--it always reflects the time
     * this volume was created (compare with the creation date--the creation date of
     * a backup volume is the creation date of the original parent, because the backup
     * is used to backup the parent volume). */
    Date copydate;
    VolumeId id, parent;
    id = to->id;
    parent = to->parentId;
    copydate = to->copyDate;
    memcpy(to, from, sizeof(*from));
    to->id = id;
    to->parentId = parent;
    to->copyDate = copydate;
    to->destroyMe = DESTROY_ME;	/* Caller must always clear this!!! */
    to->stamp.magic = VOLUMEINFOMAGIC;
    to->stamp.version = VOLUMEINFOVERSION;
    return 0;
}

afs_int32
CopyVolumeHeader(VolumeDiskData * from, VolumeDiskData * to)
{
    afs_int32 code;

    VOL_LOCK;
    code = CopyVolumeHeader_r(from, to);
    VOL_UNLOCK;
    return (code);
}

void
ClearVolumeStats(VolumeDiskData * vol)
{
    VOL_LOCK;
    ClearVolumeStats_r(vol);
    VOL_UNLOCK;
}

void
ClearVolumeStats_r(VolumeDiskData * vol)
{
    memset(vol->weekUse, 0, sizeof(vol->weekUse));
    vol->dayUse = 0;
    vol->dayUseDate = 0;
}

void
CopyVolumeStats_r(VolumeDiskData * from, VolumeDiskData * to)
{
    memcpy(to->weekUse, from->weekUse, sizeof(to->weekUse));
    to->dayUse = from->dayUse;
    to->dayUseDate = from->dayUseDate;
    if (from->stat_initialized) {
	memcpy(to->stat_reads, from->stat_reads, sizeof(to->stat_reads));
	memcpy(to->stat_writes, from->stat_writes, sizeof(to->stat_writes));
	memcpy(to->stat_fileSameAuthor, from->stat_fileSameAuthor,
	       sizeof(to->stat_fileSameAuthor));
	memcpy(to->stat_fileDiffAuthor, from->stat_fileDiffAuthor,
	       sizeof(to->stat_fileDiffAuthor));
	memcpy(to->stat_dirSameAuthor, from->stat_dirSameAuthor,
	       sizeof(to->stat_dirSameAuthor));
	memcpy(to->stat_dirDiffAuthor, from->stat_dirDiffAuthor,
	       sizeof(to->stat_dirDiffAuthor));
    }
}

void
CopyVolumeStats(VolumeDiskData * from, VolumeDiskData * to)
{
    VOL_LOCK;
    CopyVolumeStats_r(from, to);
    VOL_UNLOCK;
}

/**
 * read an existing volume disk header.
 *
 * @param[in]  volid  volume id
 * @param[in]  dp     disk partition object
 * @param[out] hdr    volume disk header or NULL
 *
 * @note if hdr is NULL, this is essentially an existence test for the vol
 *       header
 *
 * @return operation status
 *    @retval 0 success
 *    @retval -1 volume header doesn't exist
 *    @retval EIO failed to read volume header
 *
 * @internal
 */
afs_int32
VReadVolumeDiskHeader(VolumeId volid,
		      struct DiskPartition64 * dp,
		      VolumeDiskHeader_t * hdr)
{
    afs_int32 code = 0;
    int fd;
    char path[MAXPATHLEN];

    (void)afs_snprintf(path, sizeof(path),
		       "%s" OS_DIRSEP VFORMAT,
		       VPartitionPath(dp), afs_printable_uint32_lu(volid));
    fd = open(path, O_RDONLY);
    if (fd < 0) {
	Log("VReadVolumeDiskHeader: Couldn't open header for volume %lu (errno %d).\n",
	    afs_printable_uint32_lu(volid), errno);
	code = -1;

    } else if (hdr && read(fd, hdr, sizeof(*hdr)) != sizeof(*hdr)) {
	Log("VReadVolumeDiskHeader: Couldn't read header for volume %lu.\n",
	    afs_printable_uint32_lu(volid));
	code = EIO;
    }

    if (fd >= 0) {
	close(fd);
    }
    return code;
}

#ifdef FSSYNC_BUILD_CLIENT
/**
 * write an existing volume disk header.
 *
 * @param[in] hdr   volume disk header
 * @param[in] dp    disk partition object
 * @param[in] cr    assert if O_CREAT | O_EXCL should be passed to open()
 *
 * @return operation status
 *    @retval 0 success
 *    @retval -1 volume header doesn't exist
 *    @retval EIO failed to write volume header
 *
 * @internal
 */
static afs_int32
_VWriteVolumeDiskHeader(VolumeDiskHeader_t * hdr,
			struct DiskPartition64 * dp,
			int flags)
{
    afs_int32 code = 0;
    int fd;
    char path[MAXPATHLEN];

#ifdef AFS_DEMAND_ATTACH_FS
    /* prevent racing with VGC scanners reading the vol header while we are
     * writing it */
    code = VPartHeaderLock(dp, READ_LOCK);
    if (code) {
	return EIO;
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    flags |= O_RDWR;

    (void)afs_snprintf(path, sizeof(path),
		       "%s" OS_DIRSEP VFORMAT,
		       VPartitionPath(dp), afs_printable_uint32_lu(hdr->id));
    fd = open(path, flags, 0644);
    if (fd < 0) {
	code = errno;
	Log("_VWriteVolumeDiskHeader: Couldn't open header for volume %lu, "
	    "error = %d\n", afs_printable_uint32_lu(hdr->id), errno);
    } else if (write(fd, hdr, sizeof(*hdr)) != sizeof(*hdr)) {
	Log("_VWriteVolumeDiskHeader: Couldn't write header for volume %lu, "
	    "error = %d\n", afs_printable_uint32_lu(hdr->id), errno);
	code = EIO;
    }

    if (fd >= 0) {
	if (close(fd) != 0) {
	    Log("_VWriteVolumeDiskHeader: Error closing header for volume "
	        "%lu, errno %d\n", afs_printable_uint32_lu(hdr->id), errno);
	}
    }

#ifdef AFS_DEMAND_ATTACH_FS
    VPartHeaderUnlock(dp, READ_LOCK);
#endif /* AFS_DEMAND_ATTACH_FS */

    return code;
}

/**
 * write an existing volume disk header.
 *
 * @param[in] hdr   volume disk header
 * @param[in] dp    disk partition object
 *
 * @return operation status
 *    @retval 0 success
 *    @retval ENOENT volume header doesn't exist
 *    @retval EIO failed to write volume header
 */
afs_int32
VWriteVolumeDiskHeader(VolumeDiskHeader_t * hdr,
		       struct DiskPartition64 * dp)
{
    afs_int32 code;

#ifdef AFS_DEMAND_ATTACH_FS
    VolumeDiskHeader_t oldhdr;
    int delvgc = 0, addvgc = 0;
    SYNC_response res;

    /* first, see if anything with the volume IDs have changed; if so, we
     * need to update the VGC */

    code = VReadVolumeDiskHeader(hdr->id, dp, &oldhdr);
    if (code == 0 && (oldhdr.id != hdr->id || oldhdr.parent != hdr->parent)) {
	/* the vol id or parent vol id changed; need to delete the VGC entry
	 * for the old vol id/parent, and add the new one */
	delvgc = 1;
	addvgc = 1;

    } else if (code) {
	/* couldn't get the old header info; add the new header info to the
	 * VGC in case it hasn't been added yet */
	addvgc = 1;
    }

#endif /* AFS_DEMAND_ATTACH_FS */

    code = _VWriteVolumeDiskHeader(hdr, dp, 0);
    if (code) {
	goto done;
    }

#ifdef AFS_DEMAND_ATTACH_FS
    if (delvgc) {
	memset(&res, 0, sizeof(res));
	code = FSYNC_VGCDel(dp->name, oldhdr.parent, oldhdr.id, FSYNC_WHATEVER, &res);

	/* unknown vol id is okay; it just further suggests the old header
	 * data was bogus, which is fine since we're trying to fix it */
	if (code && res.hdr.reason != FSYNC_UNKNOWN_VOLID) {
	    Log("VWriteVolumeDiskHeader: FSYNC_VGCDel(%s, %lu, %lu) "
	        "failed with code %ld reason %ld\n", dp->name,
	        afs_printable_uint32_lu(oldhdr.parent),
	        afs_printable_uint32_lu(oldhdr.id),
	        afs_printable_int32_ld(code),
	        afs_printable_int32_ld(res.hdr.reason));
	}

    }
    if (addvgc) {
	memset(&res, 0, sizeof(res));
	code = FSYNC_VGCAdd(dp->name, hdr->parent, hdr->id, FSYNC_WHATEVER, &res);
	if (code) {
	    Log("VWriteVolumeDiskHeader: FSYNC_VGCAdd(%s, %lu, %lu) "
	        "failed with code %ld reason %ld\n", dp->name,
	        afs_printable_uint32_lu(hdr->parent),
	        afs_printable_uint32_lu(hdr->id),
	        afs_printable_int32_ld(code),
		afs_printable_int32_ld(res.hdr.reason));
	}
    }

#endif /* AFS_DEMAND_ATTACH_FS */

 done:
    return code;
}

/**
 * create and write a volume disk header to disk.
 *
 * @param[in] hdr   volume disk header
 * @param[in] dp    disk partition object
 *
 * @return operation status
 *    @retval 0 success
 *    @retval EEXIST volume header already exists
 *    @retval EIO failed to write volume header
 *
 * @internal
 */
afs_int32
VCreateVolumeDiskHeader(VolumeDiskHeader_t * hdr,
			struct DiskPartition64 * dp)
{
    afs_int32 code = 0;
#ifdef AFS_DEMAND_ATTACH_FS
    SYNC_response res;
#endif /* AFS_DEMAND_ATTACH_FS */

    code = _VWriteVolumeDiskHeader(hdr, dp, O_CREAT | O_EXCL);
    if (code) {
	goto done;
    }

#ifdef AFS_DEMAND_ATTACH_FS
    memset(&res, 0, sizeof(res));
    code = FSYNC_VGCAdd(dp->name, hdr->parent, hdr->id, FSYNC_WHATEVER, &res);
    if (code) {
	Log("VCreateVolumeDiskHeader: FSYNC_VGCAdd(%s, %lu, %lu) failed "
	    "with code %ld reason %ld\n", dp->name,
	    afs_printable_uint32_lu(hdr->parent),
	    afs_printable_uint32_lu(hdr->id),
	    afs_printable_int32_ld(code),
	    afs_printable_int32_ld(res.hdr.reason));
    }
#endif /* AFS_DEMAND_ATTACH_FS */

 done:
    return code;
}


/**
 * destroy a volume disk header.
 *
 * @param[in] dp      disk partition object
 * @param[in] volid   volume id
 * @param[in] parent  parent's volume id, 0 if unknown
 *
 * @return operation status
 *    @retval 0 success
 *
 * @note if parent is 0, the parent volume ID will be looked up from the
 * fileserver
 *
 * @note for non-DAFS, parent is currently ignored
 */
afs_int32
VDestroyVolumeDiskHeader(struct DiskPartition64 * dp,
			 VolumeId volid,
			 VolumeId parent)
{
    afs_int32 code = 0;
    char path[MAXPATHLEN];
#ifdef AFS_DEMAND_ATTACH_FS
    SYNC_response res;
#endif /* AFS_DEMAND_ATTACH_FS */

    (void)afs_snprintf(path, sizeof(path),
                       "%s" OS_DIRSEP VFORMAT,
                       VPartitionPath(dp), afs_printable_uint32_lu(volid));
    code = unlink(path);
    if (code) {
	Log("VDestroyVolumeDiskHeader: Couldn't unlink disk header, error = %d\n", errno);
	goto done;
    }

#ifdef AFS_DEMAND_ATTACH_FS
    memset(&res, 0, sizeof(res));
    if (!parent) {
	FSSYNC_VGQry_response_t q_res;

	code = FSYNC_VGCQuery(dp->name, volid, &q_res, &res);
	if (code) {
	    Log("VDestroyVolumeDiskHeader: FSYNC_VGCQuery(%s, %lu) failed "
	        "with code %ld, reason %ld\n", dp->name,
	        afs_printable_uint32_lu(volid), afs_printable_int32_ld(code),
		afs_printable_int32_ld(res.hdr.reason));
	    goto done;
	}

	parent = q_res.rw;

    }
    code = FSYNC_VGCDel(dp->name, parent, volid, FSYNC_WHATEVER, &res);
    if (code) {
	Log("VDestroyVolumeDiskHeader: FSYNC_VGCDel(%s, %lu, %lu) failed "
	    "with code %ld reason %ld\n", dp->name,
	    afs_printable_uint32_lu(parent),
	    afs_printable_uint32_lu(volid),
	    afs_printable_int32_ld(code),
	    afs_printable_int32_ld(res.hdr.reason));
    }
#endif /* AFS_DEMAND_ATTACH_FS */

 done:
    return code;
}
#endif /* FSSYNC_BUILD_CLIENT */

/**
 * handle a single vol header as part of VWalkVolumeHeaders.
 *
 * @param[in] dp      disk partition
 * @param[in] volfunc function to call when a vol header is successfully read
 * @param[in] name    full path name to the .vol header
 * @param[out] hdr    header data read in from the .vol header
 * @param[in] locked  1 if the partition headers are locked, 0 otherwise
 * @param[in] rock    the rock to pass to volfunc
 *
 * @return operation status
 *  @retval 0  success
 *  @retval -1 fatal error, stop scanning
 *  @retval 1  failed to read header
 *  @retval 2  volfunc callback indicated error after header read
 */
static int
_VHandleVolumeHeader(struct DiskPartition64 *dp, VWalkVolFunc volfunc,
                     const char *name, struct VolumeDiskHeader *hdr,
                     int locked, void *rock)
{
    int error = 0;
    int fd;

    if ((fd = afs_open(name, O_RDONLY)) == -1
        || read(fd, hdr, sizeof(*hdr))
        != sizeof(*hdr)
        || hdr->stamp.magic != VOLUMEHEADERMAGIC) {
        error = 1;
    }

    if (fd >= 0) {
	close(fd);
    }

#ifdef AFSFS_DEMAND_ATTACH_FS
    if (locked) {
	VPartHeaderUnlock(dp);
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    if (!error && volfunc) {
	/* the volume header seems fine; call the caller-supplied
	 * 'we-found-a-volume-header' function */
	int last = 1;

#ifdef AFS_DEMAND_ATTACH_FS
	if (!locked) {
	    last = 0;
	}
#endif /* AFS_DEMAND_ATTACH_FS */

	error = (*volfunc) (dp, name, hdr, last, rock);
	if (error < 0) {
	    return -1;
	}
	if (error) {
	    error = 2;
	}
    }

#ifdef AFS_DEMAND_ATTACH_FS
    if (error && !locked) {
	int code;
	/* retry reading the volume header under the partition
	 * header lock, just to be safe and ensure we're not
	 * racing something rewriting the vol header */
	code = VPartHeaderLock(dp, WRITE_LOCK);
	if (code) {
	    Log("Error acquiring partition write lock when "
		"looking at header %s\n", name);
	    return -1;
	}

	return _VHandleVolumeHeader(dp, volfunc, name, hdr, 1, rock);
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    return error;
}

/**
 * walk through the list of volume headers on a partition.
 *
 * This function looks through all of the .vol headers on a partition, reads in
 * each header, and calls the supplied volfunc function on each one. If the
 * header cannot be read (or volfunc returns a positive error code), DAFS will
 * VPartHeaderExLock() and retry. If that fails, or if we are non-DAFS, errfunc
 * will be called (which typically will unlink the problem volume header).
 *
 * If volfunc returns a negative error code, walking the partition will stop
 * and we will return an error immediately.
 *
 * @param[in] dp       partition to walk
 * @param[in] partpath the path opendir()
 * @param[in] volfunc  the function to call when a header is encountered, or
 *                     NULL to just skip over valid headers
 * @param[in] errfunc  the function to call when a problematic header is
 *                     encountered, or NULL to just skip over bad headers
 * @param[in] rock     rock for volfunc and errfunc
 *
 * @see VWalkVolFunc
 * @see VWalkErrFunc
 *
 * @return operation status
 *  @retval 0 success
 *  @retval negative fatal error, walk did not finish
 */
int
VWalkVolumeHeaders(struct DiskPartition64 *dp, const char *partpath,
                   VWalkVolFunc volfunc, VWalkErrFunc errfunc, void *rock)
{
    DIR *dirp = NULL;
    struct dirent *dentry = NULL;
    int code = 0;
    struct VolumeDiskHeader diskHeader;

    dirp = opendir(partpath);
    if (!dirp) {
	Log("VWalkVolumeHeaders: cannot open directory %s\n", partpath);
	code = -1;
	goto done;
    }

    while ((dentry = readdir(dirp))) {
	char *p = dentry->d_name;
	p = strrchr(dentry->d_name, '.');
	if (p != NULL && strcmp(p, VHDREXT) == 0) {
	    char name[VMAXPATHLEN];

	    sprintf(name, "%s" OS_DIRSEP "%s", partpath, dentry->d_name);

	    code = _VHandleVolumeHeader(dp, volfunc, name, &diskHeader, -1, rock);
	    if (code < 0) {
		/* fatal error, stop walking */
		goto done;
	    }
	    if (code && errfunc) {
		/* error with header; call the caller-supplied vol error
		 * function */

		struct VolumeDiskHeader *hdr = &diskHeader;
		if (code == 1) {
		    /* we failed to read the header at all, so don't pass in
		     * the header ptr */
		    hdr = NULL;
		}
		(*errfunc) (dp, name, hdr, rock);
	    }
	    code = 0;
	}
    }
 done:
    if (dirp) {
	closedir(dirp);
	dirp = NULL;
    }

    return code;
}

/**
 * initialize a struct VLockFile.
 *
 * @param[in] lf   struct VLockFile to initialize
 * @param[in] path Full path to the file to use for locks. The string contents
 *                 are copied.
 */
void
VLockFileInit(struct VLockFile *lf, const char *path)
{
    memset(lf, 0, sizeof(*lf));
    lf->path = strdup(path);
    lf->fd = INVALID_FD;
    MUTEX_INIT(&lf->mutex, "vlockfile", MUTEX_DEFAULT, 0);
}

#ifdef AFS_NT40_ENV
static_inline FD_t
_VOpenPath(const char *path)
{
    HANDLE handle;

    handle = CreateFile(path,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_ALWAYS,
                        FILE_ATTRIBUTE_HIDDEN,
                        NULL);
    if (handle == INVALID_HANDLE_VALUE) {
	return INVALID_FD;
    }

    return handle;
}

static_inline int
_VLockFd(FD_t handle, afs_uint32 offset, int locktype, int nonblock)
{
    DWORD flags = 0;
    OVERLAPPED lap;

    if (locktype == WRITE_LOCK) {
	flags |= LOCKFILE_EXCLUSIVE_LOCK;
    }
    if (nonblock) {
	flags |= LOCKFILE_FAIL_IMMEDIATELY;
    }

    memset(&lap, 0, sizeof(lap));
    lap.Offset = offset;

    if (!LockFileEx(handle, flags, 0, 1, 0, &lap)) {
	if (GetLastError() == ERROR_LOCK_VIOLATION) {
	    return EBUSY;
	}
	return EIO;
    }

    return 0;
}

static_inline void
_VUnlockFd(struct VLockFile *lf, afs_uint32 offset)
{
    OVERLAPPED lap;

    memset(&lap, 0, sizeof(lap));
    lap.Offset = offset;

    UnlockFileEx(lf->fd, 0, 1, 0, &lap);
}

static_inline void
_VCloseFd(struct VLockFile *lf)
{
    CloseHandle(lf->fd);
}

#else /* !AFS_NT40_ENV */

/**
 * open a file on the local filesystem suitable for locking
 *
 * @param[in] path  abs path of the file to open
 *
 * @return file descriptor
 *  @retval INVALID_FD failure opening file
 */
static_inline int
_VOpenPath(const char *path)
{
    int fd;

    fd = open(path, O_RDWR | O_CREAT, 0660);
    if (fd < 0) {
	return INVALID_FD;
    }
    return fd;
}

/**
 * lock an offset in a file descriptor.
 *
 * @param[in] fd       file descriptor to lock
 * @param[in] offset   offset in file to lock
 * @param[in] locktype READ_LOCK or WRITE_LOCK
 * @param[in] nonblock 1 to fail immediately, 0 to wait to acquire lock
 *
 * @return operation status
 *  @retval 0 success
 *  @retval EBUSY someone else is holding a conflicting lock and nonblock=1 was
 *                specified
 *  @retval EIO   error acquiring file lock
 */
static_inline int
_VLockFd(int fd, afs_uint32 offset, int locktype, int nonblock)
{
    int l_type = F_WRLCK;
    int cmd = AFS_SETLKW;
    struct afs_st_flock sf;

    if (locktype == READ_LOCK) {
	l_type = F_RDLCK;
    }
    if (nonblock) {
	cmd = AFS_SETLK;
    }

    sf.l_start = offset;
    sf.l_len = 1;
    sf.l_type = l_type;
    sf.l_whence = SEEK_SET;

    if (fcntl(fd, cmd, &sf)) {
	if (nonblock && (errno == EACCES || errno == EAGAIN)) {
	    /* We asked for a nonblocking lock, and it was already locked */
	    sf.l_pid = 0;
	    if (fcntl(fd, F_GETLK, &sf) != 0 || sf.l_pid == 0) {
		Log("_VLockFd: fcntl failed with error %d when trying to "
		    "query the conflicting lock for fd %d (locktype=%d, "
		    "offset=%lu)\n", errno, fd, locktype,
		    afs_printable_uint32_lu(offset));
	    } else {
		Log("_VLockFd: conflicting lock held on fd %d, offset %lu by "
		    "pid %ld (locktype=%d)\n", fd,
		    afs_printable_uint32_lu(offset), (long int)sf.l_pid,
		    locktype);
	    }
	    return EBUSY;
	}
	Log("_VLockFd: fcntl failed with error %d when trying to lock "
	    "fd %d (locktype=%d, offset=%lu)\n", errno, fd, locktype,
	    afs_printable_uint32_lu(offset));
	return EIO;
    }

    return 0;
}

/**
 * close a file descriptor used for file locking.
 *
 * @param[in] fd file descriptor to close
 */
static_inline void
_VCloseFd(int fd)
{
    if (close(fd)) {
	Log("_VCloseFd: error %d closing fd %d\n",
            errno, fd);
    }
}

/**
 * unlock a file offset in a file descriptor.
 *
 * @param[in] fd file descriptor to unlock
 * @param[in] offset offset to unlock
 */
static_inline void
_VUnlockFd(int fd, afs_uint32 offset)
{
    struct afs_st_flock sf;

    sf.l_start = offset;
    sf.l_len = 1;
    sf.l_type = F_UNLCK;
    sf.l_whence = SEEK_SET;

    if (fcntl(fd, AFS_SETLK, &sf)) {
	Log("_VUnlockFd: fcntl failed with error %d when trying to unlock "
	    "fd %d\n", errno, fd);
    }
}
#endif /* !AFS_NT40_ENV */

/**
 * reinitialize a struct VLockFile.
 *
 * Use this to close the lock file (unlocking any locks in it), and effectively
 * restore lf to the state it was in when it was initialized. This is the same
 * as unlocking all of the locks on the file, without having to remember what
 * all of the locks were. Do not unlock previously held locks after calling
 * this.
 *
 * @param[in] lf  struct VLockFile to reinit
 *
 * @pre nobody is waiting for a lock on this lockfile or otherwise using
 *      this lockfile at all
 */
void
VLockFileReinit(struct VLockFile *lf)
{
    MUTEX_ENTER(&lf->mutex);

    if (lf->fd != INVALID_FD) {
	_VCloseFd(lf->fd);
	lf->fd = INVALID_FD;
    }

    lf->refcount = 0;

    MUTEX_EXIT(&lf->mutex);
}

/**
 * lock a file on disk for the process.
 *
 * @param[in] lf       the struct VLockFile representing the file to lock
 * @param[in] offset   the offset in the file to lock
 * @param[in] locktype READ_LOCK or WRITE_LOCK
 * @param[in] nonblock 0 to wait for conflicting locks to clear before
 *                     obtaining the lock; 1 to fail immediately if a
 *                     conflicting lock is held by someone else
 *
 * @return operation status
 *  @retval 0 success
 *  @retval EBUSY someone else is holding a conflicting lock and nonblock=1 was
 *                specified
 *  @retval EIO   error acquiring file lock
 *
 * @note DAFS only
 *
 * @note do not try to lock/unlock the same offset in the same file from
 * different threads; use VGetDiskLock to protect threads from each other in
 * addition to other processes
 */
int
VLockFileLock(struct VLockFile *lf, afs_uint32 offset, int locktype, int nonblock)
{
    int code;

    osi_Assert(locktype == READ_LOCK || locktype == WRITE_LOCK);

    MUTEX_ENTER(&lf->mutex);

    if (lf->fd == INVALID_FD) {
	lf->fd = _VOpenPath(lf->path);
	if (lf->fd == INVALID_FD) {
	    MUTEX_EXIT(&lf->mutex);
	    return EIO;
	}
    }

    lf->refcount++;

    MUTEX_EXIT(&lf->mutex);

    code = _VLockFd(lf->fd, offset, locktype, nonblock);

    if (code) {
	MUTEX_ENTER(&lf->mutex);
	if (--lf->refcount < 1) {
	    _VCloseFd(lf->fd);
	    lf->fd = INVALID_FD;
	}
	MUTEX_EXIT(&lf->mutex);
    }

    return code;
}

void
VLockFileUnlock(struct VLockFile *lf, afs_uint32 offset)
{
    MUTEX_ENTER(&lf->mutex);

    osi_Assert(lf->fd != INVALID_FD);

    if (--lf->refcount < 1) {
	_VCloseFd(lf->fd);
	lf->fd = INVALID_FD;
    } else {
	_VUnlockFd(lf->fd, offset);
    }

    MUTEX_EXIT(&lf->mutex);
}

#ifdef AFS_DEMAND_ATTACH_FS

/**
 * initialize a struct VDiskLock.
 *
 * @param[in] dl struct VDiskLock to initialize
 * @param[in] lf the struct VLockFile to associate with this disk lock
 */
void
VDiskLockInit(struct VDiskLock *dl, struct VLockFile *lf, afs_uint32 offset)
{
    osi_Assert(lf);
    memset(dl, 0, sizeof(*dl));
    Lock_Init(&dl->rwlock);
    MUTEX_INIT(&dl->mutex, "disklock", MUTEX_DEFAULT, 0);
    CV_INIT(&dl->cv, "disklock cv", CV_DEFAULT, 0);
    dl->lockfile = lf;
    dl->offset = offset;
}

/**
 * acquire a lock on a file on local disk.
 *
 * @param[in] dl       the VDiskLock structure corresponding to the file on disk
 * @param[in] locktype READ_LOCK if you want a read lock, or WRITE_LOCK if
 *                     you want a write lock
 * @param[in] nonblock 0 to wait for conflicting locks to clear before
 *                     obtaining the lock; 1 to fail immediately if a
 *                     conflicting lock is held by someone else
 *
 * @return operation status
 *  @retval 0 success
 *  @retval EBUSY someone else is holding a conflicting lock and nonblock=1 was
 *                specified
 *  @retval EIO   error acquiring file lock
 *
 * @note DAFS only
 *
 * @note while normal fcntl-y locks on Unix systems generally only work per-
 * process, this interface also deals with locks between threads in the
 * process in addition to different processes acquiring the lock
 */
int
VGetDiskLock(struct VDiskLock *dl, int locktype, int nonblock)
{
    int code = 0;
    osi_Assert(locktype == READ_LOCK || locktype == WRITE_LOCK);

    if (nonblock) {
	if (locktype == READ_LOCK) {
	    ObtainReadLockNoBlock(&dl->rwlock, code);
	} else {
	    ObtainWriteLockNoBlock(&dl->rwlock, code);
	}

	if (code) {
	    return EBUSY;
	}

    } else if (locktype == READ_LOCK) {
	ObtainReadLock(&dl->rwlock);
    } else {
	ObtainWriteLock(&dl->rwlock);
    }

    MUTEX_ENTER(&dl->mutex);

    if ((dl->flags & VDISKLOCK_ACQUIRING)) {
	/* Some other thread is waiting to acquire an fs lock. If nonblock=1,
	 * we can return immediately, since we know we'll need to wait to
	 * acquire. Otherwise, wait for the other thread to finish acquiring
	 * the fs lock */
	if (nonblock) {
	    code = EBUSY;
	} else {
	    while ((dl->flags & VDISKLOCK_ACQUIRING)) {
		CV_WAIT(&dl->cv, &dl->mutex);
	    }
	}
    }

    if (code == 0 && !(dl->flags & VDISKLOCK_ACQUIRED)) {
	/* no other thread holds the lock on the actual file; so grab one */

	/* first try, don't block on the lock to see if we can get it without
	 * waiting */
	code = VLockFileLock(dl->lockfile, dl->offset, locktype, 1);

	if (code == EBUSY && !nonblock) {

	    /* mark that we are waiting on the fs lock */
	    dl->flags |= VDISKLOCK_ACQUIRING;

	    MUTEX_EXIT(&dl->mutex);
	    code = VLockFileLock(dl->lockfile, dl->offset, locktype, nonblock);
	    MUTEX_ENTER(&dl->mutex);

	    dl->flags &= ~VDISKLOCK_ACQUIRING;

	    if (code == 0) {
		dl->flags |= VDISKLOCK_ACQUIRED;
	    }

	    CV_BROADCAST(&dl->cv);
	}
    }

    if (code) {
	if (locktype == READ_LOCK) {
	    ReleaseReadLock(&dl->rwlock);
	} else {
	    ReleaseWriteLock(&dl->rwlock);
	}
    } else {
	/* successfully got the lock, so inc the number of unlocks we need
	 * to do before we can unlock the actual file */
	++dl->lockers;
    }

    MUTEX_EXIT(&dl->mutex);

    return code;
}

/**
 * release a lock on a file on local disk.
 *
 * @param[in] dl the struct VDiskLock to release
 * @param[in] locktype READ_LOCK if you are unlocking a read lock, or
 *                     WRITE_LOCK if you are unlocking a write lock
 *
 * @return operation status
 *  @retval 0 success
 */
void
VReleaseDiskLock(struct VDiskLock *dl, int locktype)
{
    osi_Assert(locktype == READ_LOCK || locktype == WRITE_LOCK);

    MUTEX_ENTER(&dl->mutex);
    osi_Assert(dl->lockers > 0);

    if (--dl->lockers < 1) {
	/* no threads are holding this lock anymore, so we can release the
	 * actual disk lock */
	VLockFileUnlock(dl->lockfile, dl->offset);
	dl->flags &= ~VDISKLOCK_ACQUIRED;
    }

    MUTEX_EXIT(&dl->mutex);

    if (locktype == READ_LOCK) {
	ReleaseReadLock(&dl->rwlock);
    } else {
	ReleaseWriteLock(&dl->rwlock);
    }
}

#endif /* AFS_DEMAND_ATTACH_FS */
