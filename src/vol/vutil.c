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

RCSID
    ("$Header$");

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
#include <sys/stat.h>
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */

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
#include "partition.h"
#include "viceinode.h"

#include "volinodes.h"
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

/*@printflike@*/ extern void Log(const char *format, ...);

void AssignVolumeName(register VolumeDiskData * vol, char *name, char *ext);
void AssignVolumeName_r(register VolumeDiskData * vol, char *name, char *ext);
void ClearVolumeStats(register VolumeDiskData * vol);
void ClearVolumeStats_r(register VolumeDiskData * vol);


#define nFILES	(sizeof (stuff)/sizeof(struct stuff))

/* Note:  the volume creation functions herein leave the destroyMe flag in the
   volume header ON:  this means that the volumes will not be attached by the
   file server and WILL BE DESTROYED the next time a system salvage is performed */

static void
RemoveInodes(Device dev, VolumeId vid)
{
    register int i;
    IHandle_t *handle;

    /* This relies on the fact that IDEC only needs the device and NT only
     * needs the dev and vid to decrement volume special files.
     */
    IH_INIT(handle, dev, vid, -1);
    for (i = 0; i < nFILES; i++) {
	Inode inode = *stuff[i].inode;
	if (VALID_INO(inode))
	    IH_DEC(handle, inode, vid);
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
    int fd, i;
    char headerName[32], volumePath[64];
    Device device;
    struct DiskPartition64 *partition;
    struct VolumeDiskHeader diskHeader;
    IHandle_t *handle;
    FdHandle_t *fdP;
    Inode nearInode = 0;

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
    VLockPartition_r(partname);
    memset(&tempHeader, 0, sizeof(tempHeader));
    tempHeader.stamp.magic = VOLUMEHEADERMAGIC;
    tempHeader.stamp.version = VOLUMEHEADERVERSION;
    tempHeader.id = vol.id;
    tempHeader.parent = vol.parentId;
    vol.stamp.magic = VOLUMEINFOMAGIC;
    vol.stamp.version = VOLUMEINFOVERSION;
    vol.destroyMe = DESTROY_ME;
    (void)afs_snprintf(headerName, sizeof headerName, VFORMAT, vol.id);
    (void)afs_snprintf(volumePath, sizeof volumePath, "%s/%s",
		       VPartitionPath(partition), headerName);
    fd = afs_open(volumePath, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd == -1) {
	if (errno == EEXIST) {
	    Log("VCreateVolume: Header file %s already exists!\n",
		volumePath);
	    *ec = VVOLEXISTS;
	} else {
	    Log("VCreateVolume: Couldn't create header file %s for volume %u\n", volumePath, vol.id);
	    *ec = VNOVOL;
	}
	return NULL;
    }
    device = partition->device;

    for (i = 0; i < nFILES; i++) {
	register struct stuff *p = &stuff[i];
	if (p->obsolete)
	    continue;
#ifdef AFS_NAMEI_ENV
	*(p->inode) =
	    IH_CREATE(NULL, device, VPartitionPath(partition), nearInode,
		      (p->inodeType == VI_LINKTABLE) ? vol.parentId : vol.id,
		      INODESPECIAL, p->inodeType, vol.parentId);
	if (!(VALID_INO(*(p->inode)))) {
	    if (errno == EEXIST) {
		/* Increment the reference count instead. */
		IHandle_t *lh;
		int code;

#ifdef AFS_NT40_ENV
		*(p->inode) = nt_MakeSpecIno(VI_LINKTABLE);
#else
		*(p->inode) = namei_MakeSpecIno(vol.parentId, VI_LINKTABLE);
#endif
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
	    RemoveInodes(device, vol.id);
	    *ec = VNOVOL;
	    close(fd);
	    return NULL;
	}
	IH_INIT(handle, device, vol.parentId, *(p->inode));
	fdP = IH_OPEN(handle);
	if (fdP == NULL) {
	    Log("VCreateVolume:  Problem iopen inode %s (err=%d)\n",
		PrintInode(NULL, *(p->inode)), errno);
	    goto bad;
	}
	if (FDH_SEEK(fdP, 0, SEEK_SET) < 0) {
	    Log("VCreateVolume:  Problem lseek inode %s (err=%d)\n",
		PrintInode(NULL, *(p->inode)), errno);
	    FDH_REALLYCLOSE(fdP);
	    goto bad;
	}
	if (FDH_WRITE(fdP, (char *)&p->stamp, sizeof(p->stamp)) !=
	    sizeof(p->stamp)) {
	    Log("VCreateVolume:  Problem writing to  inode %s (err=%d)\n",
		PrintInode(NULL, *(p->inode)), errno);
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
	    PrintInode(NULL, tempHeader.volumeInfo), errno);
	unlink(volumePath);
	goto bad;
    }
    if (FDH_SEEK(fdP, 0, SEEK_SET) < 0) {
	Log("VCreateVolume:  Problem lseek inode %s (err=%d)\n",
	    PrintInode(NULL, tempHeader.volumeInfo), errno);
	FDH_REALLYCLOSE(fdP);
	unlink(volumePath);
	goto bad;
    }
    if (FDH_WRITE(fdP, (char *)&vol, sizeof(vol)) != sizeof(vol)) {
	Log("VCreateVolume:  Problem writing to  inode %s (err=%d)\n",
	    PrintInode(NULL, tempHeader.volumeInfo), errno);
	FDH_REALLYCLOSE(fdP);
	unlink(volumePath);
	goto bad;
    }
    FDH_CLOSE(fdP);
    IH_RELEASE(handle);

    VolumeHeaderToDisk(&diskHeader, &tempHeader);
    if (write(fd, &diskHeader, sizeof(diskHeader)) != sizeof(diskHeader)) {
	Log("VCreateVolume: Unable to write volume header %s; volume %u not created\n", volumePath, vol.id);
	unlink(volumePath);
	goto bad;
    }
    fsync(fd);
    close(fd);
    return (VAttachVolumeByName_r(ec, partname, headerName, V_SECRETLY));
}


void
AssignVolumeName(register VolumeDiskData * vol, char *name, char *ext)
{
    VOL_LOCK;
    AssignVolumeName_r(vol, name, ext);
    VOL_UNLOCK;
}

void
AssignVolumeName_r(register VolumeDiskData * vol, char *name, char *ext)
{
    register char *dot;
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
ClearVolumeStats(register VolumeDiskData * vol)
{
    VOL_LOCK;
    ClearVolumeStats_r(vol);
    VOL_UNLOCK;
}

void
ClearVolumeStats_r(register VolumeDiskData * vol)
{
    memset(vol->weekUse, 0, sizeof(vol->weekUse));
    vol->dayUse = 0;
    vol->dayUseDate = 0;
}
