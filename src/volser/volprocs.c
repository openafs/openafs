/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <dirent.h>
#include <sys/stat.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <afs/afsint.h>
#include <signal.h>
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */
#include <afs/prs_fs.h>
#include <afs/nfs.h>
#include <lwp.h>
#include <lock.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/ihandle.h>
#ifdef AFS_NT40_ENV
#include <afs/ntops.h>
#endif
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/partition.h>
#include "vol.h"
#include <afs/daemon_com.h>
#include <afs/fssync.h>
#include <afs/acl.h>
#include "afs/audit.h"
#include <afs/dir.h>

#include "volser.h"
#include "volint.h"

#include "volser_prototypes.h"

extern int DoLogging;
extern struct volser_trans *FindTrans(), *NewTrans(), *TransList();
extern struct afsconf_dir *tdir;

/* Needed by Irix. Leave, or include a header */
extern char *volutil_PartitionName();

extern void LogError(afs_int32 errcode);

/* Forward declarations */
static int GetPartName(afs_int32 partid, char *pname);

#define OneDay (24*60*60)

#ifdef AFS_NT40_ENV
#define ENOTCONN 134
#endif

afs_int32 localTid = 1;
afs_int32 VolPartitionInfo(), VolNukeVolume(), VolCreateVolume(),
VolDeleteVolume(), VolClone();
afs_int32 VolReClone(), VolTransCreate(), VolGetNthVolume(), VolGetFlags(),
VolForward(), VolDump();
afs_int32 VolRestore(), VolEndTrans(), VolSetForwarding(), VolGetStatus(),
VolSetInfo(), VolGetName();
afs_int32 VolListPartitions(), VolListOneVolume(),
VolXListOneVolume(), VolXListVolumes();
afs_int32 VolListVolumes(), XVolListPartitions(), VolMonitor(),
VolSetIdsTypes(), VolSetDate(), VolSetFlags();

/* this call unlocks all of the partition locks we've set */
int 
VPFullUnlock()
{
    register struct DiskPartition *tp;
    for (tp = DiskPartitionList; tp; tp = tp->next) {
	if (tp->lock_fd != -1) {
	    close(tp->lock_fd);	/* releases flock held on this partition */
	    tp->lock_fd = -1;
	}
    }
    return 0;
}

/* get partition id from a name */
afs_int32
PartitionID(char *aname)
{
    register char tc;
    register int code = 0;
    char ascii[3];

    tc = *aname;
    if (tc == 0)
	return -1;		/* unknown */

    /* otherwise check for vicepa or /vicepa, or just plain "a" */
    ascii[2] = 0;
    if (!strncmp(aname, "/vicep", 6)) {
	strncpy(ascii, aname + 6, 2);
    } else
	return -1;		/* bad partition name */
    /* now partitions are named /vicepa ... /vicepz, /vicepaa, /vicepab, .../vicepzz, and are numbered
     * from 0.  Do the appropriate conversion */
    if (ascii[1] == 0) {
	/* one char name, 0..25 */
	if (ascii[0] < 'a' || ascii[0] > 'z')
	    return -1;		/* wrongo */
	return ascii[0] - 'a';
    } else {
	/* two char name, 26 .. <whatever> */
	if (ascii[0] < 'a' || ascii[0] > 'z')
	    return -1;		/* wrongo */
	if (ascii[1] < 'a' || ascii[1] > 'z')
	    return -1;		/* just as bad */
	code = (ascii[0] - 'a') * 26 + (ascii[1] - 'a') + 26;
	if (code > VOLMAXPARTS)
	    return -1;
	return code;
    }
}

static int
ConvertVolume(afs_int32 avol, char *aname, afs_int32 asize)
{
    if (asize < 18)
	return -1;
    /* It's better using the Generic VFORMAT since otherwise we have to make changes to too many places... The 14 char limitation in names hits us again in AIX; print in field of 9 digits (still 10 for the rest), right justified with 0 padding */
    (void)afs_snprintf(aname, asize, VFORMAT, (unsigned long)avol);
    return 0;
}

static int
ConvertPartition(int apartno, char *aname, int asize)
{
    if (asize < 10)
	return E2BIG;
    if (apartno < 0)
	return EINVAL;
    strcpy(aname, "/vicep");
    if (apartno < 26) {
	aname[6] = 'a' + apartno;
	aname[7] = 0;
    } else {
	apartno -= 26;
	aname[6] = 'a' + (apartno / 26);
	aname[7] = 'a' + (apartno % 26);
	aname[8] = 0;
    }
    return 0;
}

/* the only attach function that takes a partition is "...ByName", so we use it */
struct Volume *
XAttachVolume(afs_int32 *error, afs_int32 avolid, afs_int32 apartid, int amode)
{
    char pbuf[30], vbuf[20];
    register struct Volume *tv;

    if (ConvertPartition(apartid, pbuf, sizeof(pbuf))) {
	*error = EINVAL;
	return NULL;
    }
    if (ConvertVolume(avolid, vbuf, sizeof(vbuf))) {
	*error = EINVAL;
	return NULL;
    }
    tv = VAttachVolumeByName(error, pbuf, vbuf, amode);
    return tv;
}

/* Adapted from the file server; create a root directory for this volume */
static int
ViceCreateRoot(Volume *vp)
{
    DirHandle dir;
    struct acl_accessList *ACL;
    ViceFid did;
    Inode inodeNumber, nearInode;
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[vLarge];
    IHandle_t *h;
    FdHandle_t *fdP;
    int code;
    afs_fsize_t length;

    memset(vnode, 0, SIZEOF_LARGEDISKVNODE);

    V_pref(vp, nearInode);
    inodeNumber =
	IH_CREATE(V_linkHandle(vp), V_device(vp),
		  VPartitionPath(V_partition(vp)), nearInode, V_parentId(vp),
		  1, 1, 0);
    assert(VALID_INO(inodeNumber));

    SetSalvageDirHandle(&dir, V_parentId(vp), vp->device, inodeNumber);
    did.Volume = V_id(vp);
    did.Vnode = (VnodeId) 1;
    did.Unique = 1;

    assert(!(MakeDir(&dir, &did, &did)));
    DFlush();			/* flush all modified dir buffers out */
    DZap(&dir);			/* Remove all buffers for this dir */
    length = Length(&dir);	/* Remember size of this directory */

    FidZap(&dir);		/* Done with the dir handle obtained via SetSalvageDirHandle() */

    /* build a single entry ACL that gives all rights to system:administrators */
    /* this section of code assumes that access list format is not going to
     * change
     */
    ACL = VVnodeDiskACL(vnode);
    ACL->size = sizeof(struct acl_accessList);
    ACL->version = ACL_ACLVERSION;
    ACL->total = 1;
    ACL->positive = 1;
    ACL->negative = 0;
    ACL->entries[0].id = -204;	/* this assumes System:administrators is group -204 */
    ACL->entries[0].rights =
	PRSFS_READ | PRSFS_WRITE | PRSFS_INSERT | PRSFS_LOOKUP | PRSFS_DELETE
	| PRSFS_LOCK | PRSFS_ADMINISTER;

    vnode->type = vDirectory;
    vnode->cloned = 0;
    vnode->modeBits = 0777;
    vnode->linkCount = 2;
    VNDISK_SET_LEN(vnode, length);
    vnode->uniquifier = 1;
    V_uniquifier(vp) = vnode->uniquifier + 1;
    vnode->dataVersion = 1;
    VNDISK_SET_INO(vnode, inodeNumber);
    vnode->unixModifyTime = vnode->serverModifyTime = V_creationDate(vp);
    vnode->author = 0;
    vnode->owner = 0;
    vnode->parent = 0;
    vnode->vnodeMagic = vcp->magic;

    IH_INIT(h, vp->device, V_parentId(vp),
	    vp->vnodeIndex[vLarge].handle->ih_ino);
    fdP = IH_OPEN(h);
    assert(fdP != NULL);
    code = FDH_SEEK(fdP, vnodeIndexOffset(vcp, 1), SEEK_SET);
    assert(code >= 0);
    code = FDH_WRITE(fdP, vnode, SIZEOF_LARGEDISKVNODE);
    assert(code == SIZEOF_LARGEDISKVNODE);
    FDH_REALLYCLOSE(fdP);
    IH_RELEASE(h);
    VNDISK_GET_LEN(length, vnode);
    V_diskused(vp) = nBlocks(length);

    return 1;
}

afs_int32
SAFSVolPartitionInfo(struct rx_call *acid, char *pname, struct diskPartition 
		     *partition)
{
    afs_int32 code;

    code = VolPartitionInfo(acid, pname, partition);
    osi_auditU(acid, VS_ParInfEvent, code, AUD_STR, pname, AUD_END);
    return code;
}

afs_int32
VolPartitionInfo(struct rx_call *acid, char *pname, struct diskPartition 
		 *partition)
{
    register struct DiskPartition *dp;

/*
    if (!afsconf_SuperUser(tdir, acid, caller)) return VOLSERBAD_ACCESS;
*/
    VResetDiskUsage();
    dp = VGetPartition(pname, 0);
    if (dp) {
	strncpy(partition->name, dp->name, 32);
	strncpy(partition->devName, dp->devName, 32);
	partition->lock_fd = dp->lock_fd;
	partition->free = dp->free;
	partition->minFree = dp->totalUsable;
	return 0;
    } else
	return VOLSERILLEGAL_PARTITION;
}

/* obliterate a volume completely, and slowly. */
afs_int32
SAFSVolNukeVolume(struct rx_call *acid, afs_int32 apartID, afs_int32 avolID)
{
    afs_int32 code;

    code = VolNukeVolume(acid, apartID, avolID);
    osi_auditU(acid, VS_NukVolEvent, code, AUD_LONG, avolID, AUD_END);
    return code;
}

afs_int32
VolNukeVolume(struct rx_call *acid, afs_int32 apartID, afs_int32 avolID)
{
    register char *tp;
    char partName[50];
    afs_int32 error;
    register afs_int32 code;
    struct Volume *tvp;
    char caller[MAXKTCNAMELEN];

    /* check for access */
    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;
    if (DoLogging)
	Log("%s is executing VolNukeVolume %u\n", caller, avolID);

    tp = volutil_PartitionName(apartID);
    if (!tp)
	return VOLSERNOVOL;
    strcpy(partName, tp);	/* remember it for later */
    /* we first try to attach the volume in update mode, so that the file
     * server doesn't try to use it (and abort) while (or after) we delete it.
     * If we don't get the volume, that's fine, too.  We just won't put it back.
     */
    tvp = XAttachVolume(&error, avolID, apartID, V_VOLUPD);
    code = nuke(partName, avolID);
    if (tvp)
	VDetachVolume(&error, tvp);
    return code;
}

/* create a new volume, with name aname, on the specified partition (1..n)
 * and of type atype (readwriteVolume, readonlyVolume, backupVolume).
 * As input, if *avolid is 0, we allocate a new volume id, otherwise we use *avolid
 * for the volume id (useful for things like volume restore).
 * Return the new volume id in *avolid.
 */
afs_int32
SAFSVolCreateVolume(struct rx_call *acid, afs_int32 apart, char *aname, 
		    afs_int32 atype, afs_int32 aparent, afs_int32 *avolid, 
		    afs_int32 *atrans)
{
    afs_int32 code;

    code =
	VolCreateVolume(acid, apart, aname, atype, aparent, avolid, atrans);
    osi_auditU(acid, VS_CrVolEvent, code, AUD_LONG, *atrans, AUD_LONG,
	       *avolid, AUD_STR, aname, AUD_LONG, atype, AUD_LONG, aparent,
	       AUD_END);
    return code;
}

afs_int32
VolCreateVolume(struct rx_call *acid, afs_int32 apart, char *aname, 
		    afs_int32 atype, afs_int32 aparent, afs_int32 *avolid, 
		    afs_int32 *atrans)
{
    afs_int32 error;
    register Volume *vp;
    afs_int32 junk;		/* discardable error code */
    register afs_int32 volumeID, doCreateRoot = 1;
    register struct volser_trans *tt;
    char ppath[30];
    char caller[MAXKTCNAMELEN];

    if (strlen(aname) > 31)
	return VOLSERBADNAME;
    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;
    if (DoLogging)
	Log("%s is executing CreateVolume '%s'\n", caller, aname);
    if ((error = ConvertPartition(apart, ppath, sizeof(ppath))))
	return error;		/*a standard unix error */
    if (atype != readwriteVolume && atype != readonlyVolume
	&& atype != backupVolume)
	return EINVAL;
    if ((volumeID = *avolid) == 0) {

	Log("1 Volser: CreateVolume: missing volume number; %s volume not created\n", aname);
	return E2BIG;

    }
    if ((aparent == volumeID) && (atype == readwriteVolume)) {
	doCreateRoot = 0;
    }
    if (aparent == 0)
	aparent = volumeID;
    tt = NewTrans(volumeID, apart);
    if (!tt) {
	Log("1 createvolume: failed to create trans\n");
	return VOLSERVOLBUSY;	/* volume already busy! */
    }
    vp = VCreateVolume(&error, ppath, volumeID, aparent);
    if (error) {
	Log("1 Volser: CreateVolume: Unable to create the volume; aborted, error code %u\n", error);
	LogError(error);
	DeleteTrans(tt, 1);
	return EIO;
    }
    V_uniquifier(vp) = 1;
    V_creationDate(vp) = V_copyDate(vp);
    V_inService(vp) = V_blessed(vp) = 1;
    V_type(vp) = atype;
    AssignVolumeName(&V_disk(vp), aname, 0);
    if (doCreateRoot)
	ViceCreateRoot(vp);
    V_destroyMe(vp) = DESTROY_ME;
    V_inService(vp) = 0;
    V_maxquota(vp) = 5000;	/* set a quota of 5000 at init time */
    VUpdateVolume(&error, vp);
    if (error) {
	Log("1 Volser: create UpdateVolume failed, code %d\n", error);
	LogError(error);
	DeleteTrans(tt, 1);
	VDetachVolume(&junk, vp);	/* rather return the real error code */
	return error;
    }
    tt->volume = vp;
    *atrans = tt->tid;
    strcpy(tt->lastProcName, "CreateVolume");
    tt->rxCallPtr = acid;
    Log("1 Volser: CreateVolume: volume %u (%s) created\n", volumeID, aname);
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;
    return 0;
}

/* delete the volume associated with this transaction */
afs_int32
SAFSVolDeleteVolume(struct rx_call *acid, afs_int32 atrans)
{
    afs_int32 code;

    code = VolDeleteVolume(acid, atrans);
    osi_auditU(acid, VS_DelVolEvent, code, AUD_LONG, atrans, AUD_END);
    return code;
}

afs_int32
VolDeleteVolume(struct rx_call *acid, afs_int32 atrans)
{
    register struct volser_trans *tt;
    afs_int32 error;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;
    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: Delete: volume %u already deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    if (DoLogging)
	Log("%s is executing Delete Volume %u\n", caller, tt->volid);
    strcpy(tt->lastProcName, "DeleteVolume");
    tt->rxCallPtr = acid;
    VPurgeVolume(&error, tt->volume);	/* don't check error code, it is not set! */
    tt->vflags |= VTDeleted;	/* so we know not to do anything else to it */
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    Log("1 Volser: Delete: volume %u deleted \n", tt->volid);
    return 0;			/* vpurgevolume doesn't set an error code */
}

/* make a clone of the volume associated with atrans, possibly giving it a new
 * number (allocate a new number if *newNumber==0, otherwise use *newNumber
 * for the clone's id).  The new clone is given the name newName.  Finally, due to
 * efficiency considerations, if purgeId is non-zero, we purge that volume when doing
 * the clone operation.  This may be useful when making new backup volumes, for instance
 * since the net result of a clone and a purge generally leaves many inode ref counts
 * the same, while doing them separately would result in far more iincs and idecs being
 * peformed (and they are slow operations).
 */
/* for efficiency reasons, sometimes faster to piggyback a purge here */
afs_int32
SAFSVolClone(struct rx_call *acid, afs_int32 atrans, afs_int32 purgeId, 
	     afs_int32 newType, char *newName, afs_int32 *newNumber)
{
    afs_int32 code;

    code = VolClone(acid, atrans, purgeId, newType, newName, newNumber);
    osi_auditU(acid, VS_CloneEvent, code, AUD_LONG, atrans, AUD_LONG, purgeId,
	       AUD_STR, newName, AUD_LONG, newType, AUD_LONG, *newNumber,
	       AUD_END);
    return code;
}

afs_int32
VolClone(struct rx_call *acid, afs_int32 atrans, afs_int32 purgeId, 
	     afs_int32 newType, char *newName, afs_int32 *newNumber)
{
    VolumeId newId;
    register struct Volume *originalvp, *purgevp, *newvp;
    Error error, code;
    register struct volser_trans *tt, *ttc;
    char caller[MAXKTCNAMELEN];

    if (strlen(newName) > 31)
	return VOLSERBADNAME;
    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    if (DoLogging)
	Log("%s is executing Clone Volume new name=%s\n", caller, newName);
    error = 0;
    originalvp = (Volume *) 0;
    purgevp = (Volume *) 0;
    newvp = (Volume *) 0;
    tt = ttc = (struct volser_trans *)0;

    if (!newNumber || !*newNumber) {
	Log("1 Volser: Clone: missing volume number for the clone; aborted\n");
	goto fail;
    }
    newId = *newNumber;

    if (newType != readonlyVolume && newType != backupVolume)
	return EINVAL;
    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: Clone: volume %u has been deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    ttc = NewTrans(newId, tt->partition);
    if (!ttc) {			/* someone is messing with the clone already */
	TRELE(tt);
	return VBUSY;
    }
    strcpy(tt->lastProcName, "Clone");
    tt->rxCallPtr = acid;


    if (purgeId) {
	purgevp = VAttachVolume(&error, purgeId, V_VOLUPD);
	if (error) {
	    Log("1 Volser: Clone: Could not attach 'purge' volume %u; clone aborted\n", purgeId);
	    goto fail;
	}
    } else {
	purgevp = NULL;
    }
    originalvp = tt->volume;
    if ((V_type(originalvp) == backupVolume)
	|| (V_type(originalvp) == readonlyVolume)) {
	Log("1 Volser: Clone: The volume to be cloned must be a read/write; aborted\n");
	error = EROFS;
	goto fail;
    }
    if ((V_destroyMe(originalvp) == DESTROY_ME) || !V_inService(originalvp)) {
	Log("1 Volser: Clone: Volume %d is offline and cannot be cloned\n",
	    V_id(originalvp));
	error = VOFFLINE;
	goto fail;
    }
    if (purgevp) {
	if (originalvp->device != purgevp->device) {
	    Log("1 Volser: Clone: Volumes %u and %u are on different devices\n", tt->volid, purgeId);
	    error = EXDEV;
	    goto fail;
	}
	if (V_type(purgevp) != readonlyVolume) {
	    Log("1 Volser: Clone: The \"purge\" volume must be a read only volume; aborted\n");
	    error = EINVAL;
	    goto fail;
	}
	if (V_type(originalvp) == readonlyVolume
	    && V_parentId(originalvp) != V_parentId(purgevp)) {
	    Log("1 Volser: Clone: Volume %u and volume %u were not cloned from the same parent volume; aborted\n", tt->volid, purgeId);
	    error = EXDEV;
	    goto fail;
	}
	if (V_type(originalvp) == readwriteVolume
	    && tt->volid != V_parentId(purgevp)) {
	    Log("1 Volser: Clone: Volume %u was not originally cloned from volume %u; aborted\n", purgeId, tt->volid);
	    error = EXDEV;
	    goto fail;
	}
    }

    error = 0;

    newvp =
	VCreateVolume(&error, originalvp->partition->name, newId,
		      V_parentId(originalvp));
    if (error) {
	Log("1 Volser: Clone: Couldn't create new volume; clone aborted\n");
	newvp = (Volume *) 0;
	goto fail;
    }
    if (newType == readonlyVolume)
	V_cloneId(originalvp) = newId;
    Log("1 Volser: Clone: Cloning volume %u to new volume %u\n", tt->volid,
	newId);
    if (purgevp)
	Log("1 Volser: Clone: Purging old read only volume %u\n", purgeId);
    CloneVolume(&error, originalvp, newvp, purgevp);
    purgevp = NULL;		/* clone releases it, maybe even if error */
    if (error) {
	Log("1 Volser: Clone: clone operation failed with code %u\n", error);
	LogError(error);
	goto fail;
    }
    if (newType == readonlyVolume) {
	AssignVolumeName(&V_disk(newvp), V_name(originalvp), ".readonly");
	V_type(newvp) = readonlyVolume;
    } else if (newType == backupVolume) {
	AssignVolumeName(&V_disk(newvp), V_name(originalvp), ".backup");
	V_type(newvp) = backupVolume;
	V_backupId(originalvp) = newId;
    }
    strcpy(newvp->header->diskstuff.name, newName);
    V_creationDate(newvp) = V_copyDate(newvp);
    ClearVolumeStats(&V_disk(newvp));
    V_destroyMe(newvp) = DESTROY_ME;
    V_inService(newvp) = 0;
    if (newType == backupVolume) {
	V_backupDate(originalvp) = V_copyDate(newvp);
	V_backupDate(newvp) = V_copyDate(newvp);
    }
    V_inUse(newvp) = 0;
    VUpdateVolume(&error, newvp);
    if (error) {
	Log("1 Volser: Clone: VUpdate failed code %u\n", error);
	LogError(error);
	goto fail;
    }
    VDetachVolume(&error, newvp);	/* allow file server to get it's hands on it */
    newvp = NULL;
    VUpdateVolume(&error, originalvp);
    if (error) {
	Log("1 Volser: Clone: original update %u\n", error);
	LogError(error);
	goto fail;
    }
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt)) {
	tt = (struct volser_trans *)0;
	error = VOLSERTRELE_ERROR;
	goto fail;
    }
    DeleteTrans(ttc, 1);
    return 0;

  fail:
    if (purgevp)
	VDetachVolume(&code, purgevp);
    if (newvp)
	VDetachVolume(&code, newvp);
    if (tt) {
	tt->rxCallPtr = (struct rx_call *)0;
	TRELE(tt);
    }
    if (ttc)
	DeleteTrans(ttc, 1);
    return error;
}

/* reclone this volume into the specified id */
afs_int32
SAFSVolReClone(struct rx_call *acid, afs_int32 atrans, afs_int32 cloneId)
{
    afs_int32 code;

    code = VolReClone(acid, atrans, cloneId);
    osi_auditU(acid, VS_ReCloneEvent, code, AUD_LONG, atrans, AUD_LONG,
	       cloneId, AUD_END);
    return code;
}

afs_int32
VolReClone(struct rx_call *acid, afs_int32 atrans, afs_int32 cloneId)
{
    register struct Volume *originalvp, *clonevp;
    Error error, code;
    afs_int32 newType;
    register struct volser_trans *tt, *ttc;
    char caller[MAXKTCNAMELEN];

    /*not a super user */
    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;
    if (DoLogging)
	Log("%s is executing Reclone Volume %u\n", caller, cloneId);
    error = 0;
    clonevp = originalvp = (Volume *) 0;
    tt = (struct volser_trans *)0;

    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolReClone: volume %u has been deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    ttc = NewTrans(cloneId, tt->partition);
    if (!ttc) {			/* someone is messing with the clone already */
	TRELE(tt);
	return VBUSY;
    }
    strcpy(tt->lastProcName, "ReClone");
    tt->rxCallPtr = acid;

    originalvp = tt->volume;
    if ((V_type(originalvp) == backupVolume)
	|| (V_type(originalvp) == readonlyVolume)) {
	Log("1 Volser: Clone: The volume to be cloned must be a read/write; aborted\n");
	error = EROFS;
	goto fail;
    }
    if ((V_destroyMe(originalvp) == DESTROY_ME) || !V_inService(originalvp)) {
	Log("1 Volser: Clone: Volume %d is offline and cannot be cloned\n",
	    V_id(originalvp));
	error = VOFFLINE;
	goto fail;
    }

    clonevp = VAttachVolume(&error, cloneId, V_VOLUPD);
    if (error) {
	Log("1 Volser: can't attach clone %d\n", cloneId);
	goto fail;
    }

    newType = V_type(clonevp);	/* type of the new volume */

    if (originalvp->device != clonevp->device) {
	Log("1 Volser: Clone: Volumes %u and %u are on different devices\n",
	    tt->volid, cloneId);
	error = EXDEV;
	goto fail;
    }
    if (V_type(clonevp) != readonlyVolume && V_type(clonevp) != backupVolume) {
	Log("1 Volser: Clone: The \"recloned\" volume must be a read only volume; aborted\n");
	error = EINVAL;
	goto fail;
    }
    if (V_type(originalvp) == readonlyVolume
	&& V_parentId(originalvp) != V_parentId(clonevp)) {
	Log("1 Volser: Clone: Volume %u and volume %u were not cloned from the same parent volume; aborted\n", tt->volid, cloneId);
	error = EXDEV;
	goto fail;
    }
    if (V_type(originalvp) == readwriteVolume
	&& tt->volid != V_parentId(clonevp)) {
	Log("1 Volser: Clone: Volume %u was not originally cloned from volume %u; aborted\n", cloneId, tt->volid);
	error = EXDEV;
	goto fail;
    }

    error = 0;
    Log("1 Volser: Clone: Recloning volume %u to volume %u\n", tt->volid,
	cloneId);
    CloneVolume(&error, originalvp, clonevp, clonevp);
    if (error) {
	Log("1 Volser: Clone: reclone operation failed with code %d\n",
	    error);
	LogError(error);
	goto fail;
    }

    /* fix up volume name and type, CloneVolume just propagated RW's */
    if (newType == readonlyVolume) {
	AssignVolumeName(&V_disk(clonevp), V_name(originalvp), ".readonly");
	V_type(clonevp) = readonlyVolume;
    } else if (newType == backupVolume) {
	AssignVolumeName(&V_disk(clonevp), V_name(originalvp), ".backup");
	V_type(clonevp) = backupVolume;
	V_backupId(originalvp) = cloneId;
    }
    /* don't do strcpy onto diskstuff.name, it's still OK from 1st clone */

    /* pretend recloned volume is a totally new instance */
    V_copyDate(clonevp) = time(0);
    V_creationDate(clonevp) = V_copyDate(clonevp);
    ClearVolumeStats(&V_disk(clonevp));
    V_destroyMe(clonevp) = 0;
    V_inService(clonevp) = 0;
    if (newType == backupVolume) {
	V_backupDate(originalvp) = V_copyDate(clonevp);
	V_backupDate(clonevp) = V_copyDate(clonevp);
    }
    V_inUse(clonevp) = 0;
    VUpdateVolume(&error, clonevp);
    if (error) {
	Log("1 Volser: Clone: VUpdate failed code %u\n", error);
	LogError(error);
	goto fail;
    }
    VDetachVolume(&error, clonevp);	/* allow file server to get it's hands on it */
    clonevp = NULL;
    VUpdateVolume(&error, originalvp);
    if (error) {
	Log("1 Volser: Clone: original update %u\n", error);
	LogError(error);
	goto fail;
    }
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt)) {
	tt = (struct volser_trans *)0;
	error = VOLSERTRELE_ERROR;
	goto fail;
    }

    DeleteTrans(ttc, 1);

    {
	struct DiskPartition *tpartp = originalvp->partition;
	FSYNC_VolOp(cloneId, tpartp->name, FSYNC_VOL_BREAKCBKS, 0, NULL);
    }
    return 0;

  fail:
    if (clonevp)
	VDetachVolume(&code, clonevp);
    if (tt) {
	tt->rxCallPtr = (struct rx_call *)0;
	TRELE(tt);
    }
    if (ttc)
	DeleteTrans(ttc, 1);
    return error;
}

/* create a new transaction, associated with volume and partition.  Type of
 * volume transaction is spec'd by iflags.  New trans id is returned in ttid.
 * See volser.h for definition of iflags (the constants are named IT*).
 */
afs_int32
SAFSVolTransCreate(struct rx_call *acid, afs_int32 volume, afs_int32 partition,
		   afs_int32 iflags, afs_int32 *ttid)
{
    afs_int32 code;

    code = VolTransCreate(acid, volume, partition, iflags, ttid);
    osi_auditU(acid, VS_TransCrEvent, code, AUD_LONG, *ttid, AUD_LONG, volume,
	       AUD_END);
    return code;
}

afs_int32
VolTransCreate(struct rx_call *acid, afs_int32 volume, afs_int32 partition,
		   afs_int32 iflags, afs_int32 *ttid)
{
    register struct volser_trans *tt;
    register Volume *tv;
    afs_int32 error, code;
    afs_int32 mode;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    if (iflags & ITCreate)
	mode = V_SECRETLY;
    else if (iflags & ITBusy)
	mode = V_CLONE;
    else if (iflags & ITReadOnly)
	mode = V_READONLY;
    else if (iflags & ITOffline)
	mode = V_VOLUPD;
    else {
	Log("1 Volser: TransCreate: Could not create trans, error %u\n",
	    EINVAL);
	LogError(EINVAL);
	return EINVAL;
    }
    tt = NewTrans(volume, partition);
    if (!tt) {
	/* can't create a transaction? put the volume back */
	Log("1 transcreate: can't create transaction\n");
	return VOLSERVOLBUSY;
    }
    tv = XAttachVolume(&error, volume, partition, mode);
    if (error) {
	/* give up */
	if (tv)
	    VDetachVolume(&code, tv);
	DeleteTrans(tt, 1);
	return error;
    }
    tt->volume = tv;
    *ttid = tt->tid;
    tt->iflags = iflags;
    tt->vflags = 0;
    strcpy(tt->lastProcName, "TransCreate");
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;
}

/* using aindex as a 0-based index, return the aindex'th volume on this server
 * Both the volume number and partition number (one-based) are returned.
 */
afs_int32
SAFSVolGetNthVolume(struct rx_call *acid, afs_int32 aindex, afs_int32 *avolume,
		    afs_int32 *apart)
{
    afs_int32 code;

    code = VolGetNthVolume(acid, aindex, avolume, apart);
    osi_auditU(acid, VS_GetNVolEvent, code, AUD_LONG, *avolume, AUD_END);
    return code;
}

afs_int32
VolGetNthVolume(struct rx_call *acid, afs_int32 aindex, afs_int32 *avolume,
		    afs_int32 *apart)
{
    Log("1 Volser: GetNthVolume: Not yet implemented\n");
    return VOLSERNO_OP;
}

/* return the volume flags (VT* constants in volser.h) associated with this
 * transaction.
 */
afs_int32
SAFSVolGetFlags(struct rx_call *acid, afs_int32 atid, afs_int32 *aflags)
{
    afs_int32 code;

    code = VolGetFlags(acid, atid, aflags);
    osi_auditU(acid, VS_GetFlgsEvent, code, AUD_LONG, atid, AUD_END);
    return code;
}

afs_int32
VolGetFlags(struct rx_call *acid, afs_int32 atid, afs_int32 *aflags)
{
    register struct volser_trans *tt;

    tt = FindTrans(atid);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolGetFlags: volume %u has been deleted \n",
	    tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "GetFlags");
    tt->rxCallPtr = acid;
    *aflags = tt->vflags;
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;
}

/* Change the volume flags (VT* constants in volser.h) associated with this
 * transaction.  Effects take place immediately on volume, although volume
 * remains attached as usual by the transaction.
 */
afs_int32
SAFSVolSetFlags(struct rx_call *acid, afs_int32 atid, afs_int32 aflags)
{
    afs_int32 code;

    code = VolSetFlags(acid, atid, aflags);
    osi_auditU(acid, VS_SetFlgsEvent, code, AUD_LONG, atid, AUD_LONG, aflags,
	       AUD_END);
    return code;
}

afs_int32
VolSetFlags(struct rx_call *acid, afs_int32 atid, afs_int32 aflags)
{
    register struct volser_trans *tt;
    register struct Volume *vp;
    afs_int32 error;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    /* find the trans */
    tt = FindTrans(atid);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolSetFlags: volume %u has been deleted \n",
	    tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "SetFlags");
    tt->rxCallPtr = acid;
    vp = tt->volume;		/* pull volume out of transaction */

    /* check if we're allowed to make any updates */
    if (tt->iflags & ITReadOnly) {
	TRELE(tt);
	return EROFS;
    }

    /* handle delete-on-salvage flag */
    if (aflags & VTDeleteOnSalvage) {
	V_destroyMe(tt->volume) = DESTROY_ME;
    } else {
	V_destroyMe(tt->volume) = 0;
    }

    if (aflags & VTOutOfService) {
	V_inService(vp) = 0;
    } else {
	V_inService(vp) = 1;
    }
    VUpdateVolume(&error, vp);
    tt->vflags = aflags;
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt) && !error)
	return VOLSERTRELE_ERROR;

    return error;
}

/* dumpS the volume associated with a particular transaction from a particular
 * date.  Send the dump to a different transaction (destTrans) on the server
 * specified by the destServer structure.
 */
afs_int32
SAFSVolForward(struct rx_call *acid, afs_int32 fromTrans, afs_int32 fromDate,
	       struct destServer *destination, afs_int32 destTrans, 
	       struct restoreCookie *cookie)
{
    afs_int32 code;

    code =
	VolForward(acid, fromTrans, fromDate, destination, destTrans, cookie);
    osi_auditU(acid, VS_ForwardEvent, code, AUD_LONG, fromTrans, AUD_HOST,
	       destination->destHost, AUD_LONG, destTrans, AUD_END);
    return code;
}

afs_int32
VolForward(struct rx_call *acid, afs_int32 fromTrans, afs_int32 fromDate,
	       struct destServer *destination, afs_int32 destTrans, 
	       struct restoreCookie *cookie)
{
    register struct volser_trans *tt;
    register afs_int32 code;
    register struct rx_connection *tcon;
    struct rx_call *tcall;
    register struct Volume *vp;
    struct rx_securityClass *securityObject;
    afs_int32 securityIndex;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    /* initialize things */
    tcon = (struct rx_connection *)0;
    tt = (struct volser_trans *)0;

    /* find the local transaction */
    tt = FindTrans(fromTrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolForward: volume %u has been deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    vp = tt->volume;
    strcpy(tt->lastProcName, "Forward");

    /* get auth info for the this connection (uses afs from ticket file) */
    code = afsconf_ClientAuth(tdir, &securityObject, &securityIndex);
    if (code) {
	TRELE(tt);
	return code;
    }

    /* make an rpc connection to the other server */
    tcon =
	rx_NewConnection(htonl(destination->destHost),
			 htons(destination->destPort), VOLSERVICE_ID,
			 securityObject, securityIndex);
    if (!tcon) {
	tt->rxCallPtr = (struct rx_call *)0;
	TRELE(tt);
	return ENOTCONN;
    }
    tcall = rx_NewCall(tcon);
    tt->rxCallPtr = tcall;
    /* start restore going.  fromdate == 0 --> doing an incremental dump/restore */
    code = StartAFSVolRestore(tcall, destTrans, (fromDate ? 1 : 0), cookie);
    if (code) {
	goto fail;
    }

    /* these next calls implictly call rx_Write when writing out data */
    code = DumpVolume(tcall, vp, fromDate, 0);	/* last field = don't dump all dirs */
    if (code)
	goto fail;
    EndAFSVolRestore(tcall);	/* probably doesn't do much */
    tt->rxCallPtr = (struct rx_call *)0;
    code = rx_EndCall(tcall, 0);
    rx_DestroyConnection(tcon);	/* done with the connection */
    tcon = NULL;
    if (code)
	goto fail;
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;

  fail:
    if (tcon) {
	(void)rx_EndCall(tcall, 0);
	rx_DestroyConnection(tcon);
    }
    if (tt) {
	tt->rxCallPtr = (struct rx_call *)0;
	TRELE(tt);
    }
    return code;
}

/* Start a dump and send it to multiple places simultaneously.
 * If this returns an error (eg, return ENOENT), it means that
 * none of the releases worked.  If this returns 0, that means 
 * that one or more of the releases worked, and the caller has
 * to examine the results array to see which one(s).
 * This will only do EITHER incremental or full, not both, so it's
 * the caller's responsibility to be sure that all the destinations
 * need just an incremental (and from the same time), if that's 
 * what we're doing. 
 */
afs_int32
SAFSVolForwardMultiple(struct rx_call *acid, afs_int32 fromTrans, afs_int32 
		       fromDate, manyDests *destinations, afs_int32 spare,
		       struct restoreCookie *cookie, manyResults *results)
{
    afs_int32 securityIndex;
    struct rx_securityClass *securityObject;
    char caller[MAXKTCNAMELEN];
    struct volser_trans *tt;
    afs_int32 ec, code, *codes;
    struct rx_connection **tcons;
    struct rx_call **tcalls;
    struct Volume *vp;
    int i, is_incremental;

    if (results)
	memset(results, 0, sizeof(manyResults));

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(fromTrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolForward: volume %u has been deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    vp = tt->volume;
    strcpy(tt->lastProcName, "ForwardMulti");

    /* (fromDate == 0) ==> full dump */
    is_incremental = (fromDate ? 1 : 0);

    i = results->manyResults_len = destinations->manyDests_len;
    results->manyResults_val = codes =
	(afs_int32 *) malloc(i * sizeof(afs_int32));
    tcons =
	(struct rx_connection **)malloc(i * sizeof(struct rx_connection *));
    tcalls = (struct rx_call **)malloc(i * sizeof(struct rx_call *));

    /* get auth info for this connection (uses afs from ticket file) */
    code = afsconf_ClientAuth(tdir, &securityObject, &securityIndex);
    if (code) {
	goto fail;		/* in order to audit each failure */
    }

    /* make connections to all the other servers */
    for (i = 0; i < destinations->manyDests_len; i++) {
	struct replica *dest = &(destinations->manyDests_val[i]);
	tcons[i] =
	    rx_NewConnection(htonl(dest->server.destHost),
			     htons(dest->server.destPort), VOLSERVICE_ID,
			     securityObject, securityIndex);
	if (!tcons[i]) {
	    codes[i] = ENOTCONN;
	} else {
	    if (!(tcalls[i] = rx_NewCall(tcons[i])))
		codes[i] = ENOTCONN;
	    else {
		codes[i] =
		    StartAFSVolRestore(tcalls[i], dest->trans, is_incremental,
				       cookie);
		if (codes[i]) {
		    (void)rx_EndCall(tcalls[i], 0);
		    tcalls[i] = 0;
		    rx_DestroyConnection(tcons[i]);
		    tcons[i] = 0;
		}
	    }
	}
    }

    /* these next calls implictly call rx_Write when writing out data */
    code = DumpVolMulti(tcalls, i, vp, fromDate, 0, codes);


  fail:
    for (i--; i >= 0; i--) {
	struct replica *dest = &(destinations->manyDests_val[i]);

	if (!code && tcalls[i] && !codes[i]) {
	    EndAFSVolRestore(tcalls[i]);
	}
	if (tcalls[i]) {
	    ec = rx_EndCall(tcalls[i], 0);
	    if (!codes[i])
		codes[i] = ec;
	}
	if (tcons[i]) {
	    rx_DestroyConnection(tcons[i]);	/* done with the connection */
	}

	osi_auditU(acid, VS_ForwardEvent, (code ? code : codes[i]), AUD_LONG,
		   fromTrans, AUD_HOST, dest->server.destHost, AUD_LONG,
		   dest->trans, AUD_END);
    }
    free(tcons);
    free(tcalls);

    if (tt) {
	tt->rxCallPtr = (struct rx_call *)0;
	if (TRELE(tt) && !code)	/* return the first code if it's set */
	    return VOLSERTRELE_ERROR;
    }

    return code;
}

afs_int32
SAFSVolDump(struct rx_call *acid, afs_int32 fromTrans, afs_int32 fromDate)
{
    afs_int32 code;

    code = VolDump(acid, fromTrans, fromDate, 0);
    osi_auditU(acid, VS_DumpEvent, code, AUD_LONG, fromTrans, AUD_END);
    return code;
}

afs_int32
SAFSVolDumpV2(struct rx_call *acid, afs_int32 fromTrans, afs_int32 fromDate, afs_int32 flags)
{
    afs_int32 code;

    code = VolDump(acid, fromTrans, fromDate, flags);
    osi_auditU(acid, VS_DumpEvent, code, AUD_LONG, fromTrans, AUD_END);
    return code;
}

afs_int32
VolDump(struct rx_call *acid, afs_int32 fromTrans, afs_int32 fromDate, afs_int32 flags)
{
    int code = 0;
    register struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(fromTrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolDump: volume %u has been deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "Dump");
    tt->rxCallPtr = acid;
    code = DumpVolume(acid, tt->volume, fromDate, (flags & VOLDUMPV2_OMITDIRS)
		      ? 0 : 1);	/* squirt out the volume's data, too */
    if (code) {
	tt->rxCallPtr = (struct rx_call *)0;
	TRELE(tt);
	return code;
    }
    tt->rxCallPtr = (struct rx_call *)0;

    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;
}

/* 
 * Ha!  No more helper process!
 */
afs_int32
SAFSVolRestore(struct rx_call *acid, afs_int32 atrans, afs_int32 aflags, 
	       struct restoreCookie *cookie)
{
    afs_int32 code;

    code = VolRestore(acid, atrans, aflags, cookie);
    osi_auditU(acid, VS_RestoreEvent, code, AUD_LONG, atrans, AUD_END);
    return code;
}

afs_int32
VolRestore(struct rx_call *acid, afs_int32 atrans, afs_int32 aflags, 
	       struct restoreCookie *cookie)
{
    register struct volser_trans *tt;
    register afs_int32 code, tcode;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolRestore: volume %u has been deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "Restore");
    tt->rxCallPtr = acid;

    DFlushVolume(V_parentId(tt->volume)); /* Ensure dir buffers get dropped */

    code = RestoreVolume(acid, tt->volume, (aflags & 1), cookie);	/* last is incrementalp */
    FSYNC_VolOp(tt->volid, NULL, FSYNC_VOL_BREAKCBKS, 0l, NULL);
    tt->rxCallPtr = (struct rx_call *)0;
    tcode = TRELE(tt);

    return (code ? code : tcode);
}

/* end a transaction, returning the transaction's final error code in rcode */
afs_int32
SAFSVolEndTrans(struct rx_call *acid, afs_int32 destTrans, afs_int32 *rcode)
{
    afs_int32 code;

    code = VolEndTrans(acid, destTrans, rcode);
    osi_auditU(acid, VS_EndTrnEvent, code, AUD_LONG, destTrans, AUD_END);
    return code;
}

afs_int32
VolEndTrans(struct rx_call *acid, afs_int32 destTrans, afs_int32 *rcode)
{
    register struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(destTrans);
    if (!tt) {
	return ENOENT;
    }
    *rcode = tt->returnCode;
    DeleteTrans(tt, 1);		/* this does an implicit TRELE */

    return 0;
}

afs_int32
SAFSVolSetForwarding(struct rx_call *acid, afs_int32 atid, afs_int32 anewsite)
{
    afs_int32 code;

    code = VolSetForwarding(acid, atid, anewsite);
    osi_auditU(acid, VS_SetForwEvent, code, AUD_LONG, atid, AUD_HOST,
	       anewsite, AUD_END);
    return code;
}

afs_int32
VolSetForwarding(struct rx_call *acid, afs_int32 atid, afs_int32 anewsite)
{
    register struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(atid);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolSetForwarding: volume %u has been deleted \n",
	    tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "SetForwarding");
    tt->rxCallPtr = acid;
    FSYNC_VolOp(tt->volid, NULL, FSYNC_VOL_MOVE, anewsite, NULL);
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;
}

afs_int32
SAFSVolGetStatus(struct rx_call *acid, afs_int32 atrans, 
		 register struct volser_status *astatus)
{
    afs_int32 code;

    code = VolGetStatus(acid, atrans, astatus);
    osi_auditU(acid, VS_GetStatEvent, code, AUD_LONG, atrans, AUD_END);
    return code;
}

afs_int32
VolGetStatus(struct rx_call *acid, afs_int32 atrans, 
		 register struct volser_status *astatus)
{
    register struct Volume *tv;
    register struct VolumeDiskData *td;
    struct volser_trans *tt;


    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolGetStatus: volume %u has been deleted \n",
	    tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "GetStatus");
    tt->rxCallPtr = acid;
    tv = tt->volume;
    if (!tv) {
	tt->rxCallPtr = (struct rx_call *)0;
	TRELE(tt);
	return ENOENT;
    }

    td = &tv->header->diskstuff;
    astatus->volID = td->id;
    astatus->nextUnique = td->uniquifier;
    astatus->type = td->type;
    astatus->parentID = td->parentId;
    astatus->cloneID = td->cloneId;
    astatus->backupID = td->backupId;
    astatus->restoredFromID = td->restoredFromId;
    astatus->maxQuota = td->maxquota;
    astatus->minQuota = td->minquota;
    astatus->owner = td->owner;
    astatus->creationDate = td->creationDate;
    astatus->accessDate = td->accessDate;
    astatus->updateDate = td->updateDate;
    astatus->expirationDate = td->expirationDate;
    astatus->backupDate = td->backupDate;
    astatus->copyDate = td->copyDate;
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;
}

afs_int32
SAFSVolSetInfo(struct rx_call *acid, afs_int32 atrans, 
	       register struct volintInfo *astatus)
{
    afs_int32 code;

    code = VolSetInfo(acid, atrans, astatus);
    osi_auditU(acid, VS_SetInfoEvent, code, AUD_LONG, atrans, AUD_END);
    return code;
}

afs_int32
VolSetInfo(struct rx_call *acid, afs_int32 atrans, 
	       register struct volintInfo *astatus)
{
    register struct Volume *tv;
    register struct VolumeDiskData *td;
    struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];
    afs_int32 error;

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolSetInfo: volume %u has been deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "SetStatus");
    tt->rxCallPtr = acid;
    tv = tt->volume;
    if (!tv) {
	tt->rxCallPtr = (struct rx_call *)0;
	TRELE(tt);
	return ENOENT;
    }

    td = &tv->header->diskstuff;
    /*
     * Add more fields as necessary
     */
    if (astatus->maxquota != -1)
	td->maxquota = astatus->maxquota;
    if (astatus->dayUse != -1)
	td->dayUse = astatus->dayUse;
    if (astatus->creationDate != -1)
	td->creationDate = astatus->creationDate;
    if (astatus->updateDate != -1)
	td->updateDate = astatus->updateDate;
    if (astatus->spare2 != -1)
	td->volUpdateCounter = (unsigned int)astatus->spare2;
    VUpdateVolume(&error, tv);
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;
    return 0;
}


afs_int32
SAFSVolGetName(struct rx_call *acid, afs_int32 atrans, char **aname)
{
    afs_int32 code;

    code = VolGetName(acid, atrans, aname);
    osi_auditU(acid, VS_GetNameEvent, code, AUD_LONG, atrans, AUD_END);
    return code;
}

afs_int32
VolGetName(struct rx_call *acid, afs_int32 atrans, char **aname)
{
    register struct Volume *tv;
    register struct VolumeDiskData *td;
    struct volser_trans *tt;
    register int len;

    *aname = NULL;
    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolGetName: volume %u has been deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "GetName");
    tt->rxCallPtr = acid;
    tv = tt->volume;
    if (!tv) {
	tt->rxCallPtr = (struct rx_call *)0;
	TRELE(tt);
	return ENOENT;
    }

    td = &tv->header->diskstuff;
    len = strlen(td->name) + 1;	/* don't forget the null */
    if (len >= SIZE) {
	tt->rxCallPtr = (struct rx_call *)0;
	TRELE(tt);
	return E2BIG;
    }
    *aname = (char *)malloc(len);
    strcpy(*aname, td->name);
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;
}

/*this is a handshake to indicate that the next call will be SAFSVolRestore
 * - a noop now !*/
afs_int32
SAFSVolSignalRestore(struct rx_call *acid, char volname[], int volType, 
		     afs_int32 parentId, afs_int32 cloneId)
{
    return 0;
}


/*return a list of all partitions on the server. The non mounted
 *partitions are returned as -1 in the corresponding slot in partIds*/
afs_int32
SAFSVolListPartitions(struct rx_call *acid, struct pIDs *partIds)
{
    afs_int32 code;

    code = VolListPartitions(acid, partIds);
    osi_auditU(acid, VS_ListParEvent, code, AUD_END);
    return code;
}

afs_int32
VolListPartitions(struct rx_call *acid, struct pIDs *partIds)
{
    char namehead[9];
    char i;

    strcpy(namehead, "/vicep");	/*7 including null terminator */

    /* Just return attached partitions. */
    namehead[7] = '\0';
    for (i = 0; i < 26; i++) {
	namehead[6] = i + 'a';
	partIds->partIds[i] = VGetPartition(namehead, 0) ? i : -1;
    }

    return 0;
}

/*return a list of all partitions on the server. The non mounted
 *partitions are returned as -1 in the corresponding slot in partIds*/
afs_int32
SAFSVolXListPartitions(struct rx_call *acid, struct partEntries *pEntries)
{
    afs_int32 code;

    code = XVolListPartitions(acid, pEntries);
    osi_auditU(acid, VS_ListParEvent, code, AUD_END);
    return code;
}

afs_int32
XVolListPartitions(struct rx_call *acid, struct partEntries *pEntries)
{
    struct stat rbuf, pbuf;
    char namehead[9];
    struct partList partList;
    struct DiskPartition *dp;
    int i, j = 0, k;

    strcpy(namehead, "/vicep");	/*7 including null terminator */

    /* Only report attached partitions */
    for (i = 0; i < VOLMAXPARTS; i++) {
#ifdef AFS_DEMAND_ATTACH_FS
	dp = VGetPartitionById(i, 0);
#else
	if (i < 26) {
	    namehead[6] = i + 'a';
	    namehead[7] = '\0';
	} else {
	    k = i - 26;
	    namehead[6] = 'a' + (k / 26);
	    namehead[7] = 'a' + (k % 26);
	    namehead[8] = '\0';
	}
	dp = VGetPartition(namehead, 0);
#endif
	if (dp)
	    partList.partId[j++] = i;
    }
    pEntries->partEntries_val = (afs_int32 *) malloc(j * sizeof(int));
    memcpy((char *)pEntries->partEntries_val, (char *)&partList,
	   j * sizeof(int));
    pEntries->partEntries_len = j;
    return 0;

}

/*extract the volume id from string vname. Its of the form " V0*<id>.vol  "*/
afs_int32
ExtractVolId(char vname[])
{
    int i;
    char name[VOLSER_MAXVOLNAME + 1];

    strcpy(name, vname);
    i = 0;
    while (name[i] == 'V' || name[i] == '0')
	i++;

    name[11] = '\0';		/* smash the "." */
    return (atol(&name[i]));
}

/*return the name of the next volume header in the directory associated with dirp and dp.
*the volume id is  returned in volid, and volume header name is returned in volname*/
int
GetNextVol(DIR * dirp, char *volname, afs_int32 * volid)
{
    struct dirent *dp;

    dp = readdir(dirp);		/*read next entry in the directory */
    if (dp) {
	if ((dp->d_name[0] == 'V') && !strcmp(&(dp->d_name[11]), VHDREXT)) {
	    *volid = ExtractVolId(dp->d_name);
	    strcpy(volname, dp->d_name);
	    return 0;		/*return the name of the file representing a volume */
	} else {
	    strcpy(volname, "");
	    return 0;		/*volname doesnot represent a volume */
	}
    } else {
	strcpy(volname, "EOD");
	return 0;		/*end of directory */
    }

}

/*return the header information about the <volid> */
afs_int32
SAFSVolListOneVolume(struct rx_call *acid, afs_int32 partid, afs_int32 
		     volumeId, volEntries *volumeInfo)
{
    afs_int32 code;

    code = VolListOneVolume(acid, partid, volumeId, volumeInfo);
    osi_auditU(acid, VS_Lst1VolEvent, code, AUD_LONG, volumeId, AUD_END);
    return code;
}

afs_int32
VolListOneVolume(struct rx_call *acid, afs_int32 partid, afs_int32 
		     volumeId, volEntries *volumeInfo)
{
    volintInfo *pntr;
    register struct Volume *tv;
    struct DiskPartition *partP;
    struct volser_trans *ttc;
    char pname[9], volname[20];
    afs_int32 error = 0;
    DIR *dirp;
    afs_int32 volid;
    int found = 0;
    unsigned int now;

    volumeInfo->volEntries_val = (volintInfo *) malloc(sizeof(volintInfo));
    pntr = volumeInfo->volEntries_val;
    volumeInfo->volEntries_len = 1;
    if (GetPartName(partid, pname))
	return VOLSERILLEGAL_PARTITION;
    if (!(partP = VGetPartition(pname, 0)))
	return VOLSERILLEGAL_PARTITION;
    dirp = opendir(VPartitionPath(partP));
    if (dirp == NULL)
	return VOLSERILLEGAL_PARTITION;
    strcpy(volname, "");
    ttc = (struct volser_trans *)0;
    tv = (Volume *) 0;		/* volume not attached */

    while (strcmp(volname, "EOD") && !found) {	/*while there are more volumes in the partition */

	if (!strcmp(volname, "")) {	/* its not a volume, fetch next file */
	    GetNextVol(dirp, volname, &volid);
	    continue;		/*back to while loop */
	}

	if (volid == volumeId) {	/*copy other things too */
	    found = 1;
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();	/*make sure that the client doesnot time out */
#endif
	    ttc = NewTrans(volid, partid);
	    if (!ttc) {
		pntr->status = VBUSY;
		pntr->volid = volid;
		goto drop;
	    }
	    tv = VAttachVolumeByName(&error, pname, volname, V_PEEK);
	    if (error) {
		pntr->status = 0;	/*things are messed up */
		strcpy(pntr->name, volname);
		pntr->volid = volid;
		Log("1 Volser: ListVolumes: Could not attach volume %u (%s:%s), error=%d\n", volid, pname, volname, error);
		goto drop;
	    }
	    if (tv->header->diskstuff.destroyMe == DESTROY_ME) {
		/*this volume will be salvaged */
		pntr->status = 0;
		strcpy(pntr->name, volname);
		pntr->volid = volid;
		Log("1 Volser: ListVolumes: Volume %u (%s) will be destroyed on next salvage\n", volid, volname);
		goto drop;
	    }

	    if (tv->header->diskstuff.needsSalvaged) {
		/*this volume will be salvaged */
		pntr->status = 0;
		strcpy(pntr->name, volname);
		pntr->volid = volid;
		Log("1 Volser: ListVolumes: Volume %u (%s) needs to be salvaged\n", volid, volname);
		goto drop;
	    }

	    /*read in the relevant info */
	    pntr->status = VOK;	/*its ok */
	    pntr->volid = tv->header->diskstuff.id;
	    strcpy(pntr->name, tv->header->diskstuff.name);
	    pntr->type = tv->header->diskstuff.type;	/*if ro volume */
	    pntr->cloneID = tv->header->diskstuff.cloneId;	/*if rw volume */
	    pntr->backupID = tv->header->diskstuff.backupId;
	    pntr->parentID = tv->header->diskstuff.parentId;
	    pntr->copyDate = tv->header->diskstuff.copyDate;
	    pntr->inUse = tv->header->diskstuff.inUse;
	    pntr->size = tv->header->diskstuff.diskused;
	    pntr->needsSalvaged = tv->header->diskstuff.needsSalvaged;
	    pntr->destroyMe = tv->header->diskstuff.destroyMe;
	    pntr->maxquota = tv->header->diskstuff.maxquota;
	    pntr->filecount = tv->header->diskstuff.filecount;
	    now = FT_ApproxTime();
	    if (now - tv->header->diskstuff.dayUseDate > OneDay)
		pntr->dayUse = 0;
	    else
		pntr->dayUse = tv->header->diskstuff.dayUse;
	    pntr->creationDate = tv->header->diskstuff.creationDate;
	    pntr->accessDate = tv->header->diskstuff.accessDate;
	    pntr->updateDate = tv->header->diskstuff.updateDate;
	    pntr->backupDate = tv->header->diskstuff.backupDate;
	    pntr->spare0 = tv->header->diskstuff.minquota;
	    pntr->spare1 =
		(long)tv->header->diskstuff.weekUse[0] +
		(long)tv->header->diskstuff.weekUse[1] +
		(long)tv->header->diskstuff.weekUse[2] +
		(long)tv->header->diskstuff.weekUse[3] +
		(long)tv->header->diskstuff.weekUse[4] +
		(long)tv->header->diskstuff.weekUse[5] +
		(long)tv->header->diskstuff.weekUse[6];
	    pntr->spare2 = V_volUpCounter(tv);
	    pntr->flags = pntr->spare3 = (long)0;
	    VDetachVolume(&error, tv);	/*free the volume */
	    tv = (Volume *) 0;
	    if (error) {
		pntr->status = 0;	/*things are messed up */
		strcpy(pntr->name, volname);
		Log("1 Volser: ListVolumes: Could not detach volume %s\n",
		    volname);
		goto drop;
	    }
	}
	GetNextVol(dirp, volname, &volid);
    }
  drop:
    if (tv) {
	VDetachVolume(&error, tv);
	tv = (Volume *) 0;
    }
    if (ttc) {
	DeleteTrans(ttc, 1);
	ttc = (struct volser_trans *)0;
    }

    closedir(dirp);
    if (found)
	return 0;
    else
	return ENODEV;
}

/*------------------------------------------------------------------------
 * EXPORTED SAFSVolXListOneVolume
 *
 * Description:
 *	Returns extended info on volume a_volID on partition a_partID.
 *
 * Arguments:
 *	a_rxCidP       : Pointer to the Rx call we're performing.
 *	a_partID       : Partition for which we want the extended list.
 *	a_volID        : Volume ID we wish to know about.
 *	a_volumeXInfoP : Ptr to the extended info blob.
 *
 * Returns:
 *	0			Successful operation
 *	VOLSERILLEGAL_PARTITION if we got a bogus partition ID
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
SAFSVolXListOneVolume(struct rx_call *a_rxCidP, afs_int32 a_partID, 
		      afs_int32 a_volID, volXEntries *a_volumeXInfoP)
{
    afs_int32 code;

    code = VolXListOneVolume(a_rxCidP, a_partID, a_volID, a_volumeXInfoP);
    osi_auditU(a_rxCidP, VS_XLst1VlEvent, code, AUD_LONG, a_volID, AUD_END);
    return code;
}

afs_int32
VolXListOneVolume(struct rx_call *a_rxCidP, afs_int32 a_partID, 
		      afs_int32 a_volID, volXEntries *a_volumeXInfoP)
{				/*SAFSVolXListOneVolume */

    volintXInfo *xInfoP;	/*Ptr to the extended vol info */
    register struct Volume *tv;	/*Volume ptr */
    struct volser_trans *ttc;	/*Volume transaction ptr */
    struct DiskPartition *partP;	/*Ptr to partition */
    char pname[9], volname[20];	/*Partition, volume names */
    afs_int32 error;		/*Error code */
    afs_int32 code;		/*Return code */
    DIR *dirp;			/*Partition directory ptr */
    afs_int32 currVolID;	/*Current volume ID */
    int found = 0;		/*Did we find the volume we need? */
    struct VolumeDiskData *volDiskDataP;	/*Ptr to on-disk volume data */
    int numStatBytes;		/*Num stat bytes to copy per volume */
    unsigned int now;

    /*
     * Set up our pointers for action, marking our structure to hold exactly
     * one entry.  Also, assume we'll fail in our quest.
     */
    a_volumeXInfoP->volXEntries_val =
	(volintXInfo *) malloc(sizeof(volintXInfo));
    xInfoP = a_volumeXInfoP->volXEntries_val;
    a_volumeXInfoP->volXEntries_len = 1;
    code = ENODEV;

    /*
     * If the partition name we've been given is bad, bogue out.
     */
    if (GetPartName(a_partID, pname))
	return (VOLSERILLEGAL_PARTITION);

    /*
     * Open the directory representing the given AFS parttion.  If we can't
     * do that, we lose.
     */
    if (!(partP = VGetPartition(pname, 0)))
	return VOLSERILLEGAL_PARTITION;
    dirp = opendir(VPartitionPath(partP));
    if (dirp == NULL)
	return (VOLSERILLEGAL_PARTITION);

    /*
     * Sweep through the partition directory, looking for the desired entry.
     * First, of course, figure out how many stat bytes to copy out of each
     * volume.
     */
    numStatBytes =
	4 * ((2 * VOLINT_STATS_NUM_RWINFO_FIELDS) +
	     (4 * VOLINT_STATS_NUM_TIME_FIELDS));
    strcpy(volname, "");
    ttc = (struct volser_trans *)0;	/*No transaction yet */
    tv = (Volume *) 0;		/*Volume not yet attached */

    while (strcmp(volname, "EOD") && !found) {
	/*
	 * If this is not a volume, move on to the next entry in the
	 * partition's directory.
	 */
	if (!strcmp(volname, "")) {
	    GetNextVol(dirp, volname, &currVolID);
	    continue;
	}

	if (currVolID == a_volID) {
	    /*
	     * We found the volume entry we're interested.  Pull out the
	     * extended information, remembering to poll (so that the client
	     * doesn't time out) and to set up a transaction on the volume.
	     */
	    found = 1;
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();
#endif
	    ttc = NewTrans(currVolID, a_partID);
	    if (!ttc) {
		/*
		 * Couldn't get a transaction on this volume; let our caller
		 * know it's busy.
		 */
		xInfoP->status = VBUSY;
		xInfoP->volid = currVolID;
		goto drop;
	    }

	    /*
	     * Attach the volume, give up on the volume if we can't.
	     */
	    tv = VAttachVolumeByName(&error, pname, volname, V_PEEK);
	    if (error) {
		xInfoP->status = 0;	/*things are messed up */
		strcpy(xInfoP->name, volname);
		xInfoP->volid = currVolID;
		Log("1 Volser: XListOneVolume: Could not attach volume %u\n",
		    currVolID);
		goto drop;
	    }

	    /*
	     * Also bag out on this volume if it's been marked as needing a
	     * salvage or to-be-destroyed.
	     */
	    volDiskDataP = &(tv->header->diskstuff);
	    if (volDiskDataP->destroyMe == DESTROY_ME) {
		xInfoP->status = 0;
		strcpy(xInfoP->name, volname);
		xInfoP->volid = currVolID;
		Log("1 Volser: XListOneVolume: Volume %u will be destroyed on next salvage\n", currVolID);
		goto drop;
	    }

	    if (volDiskDataP->needsSalvaged) {
		xInfoP->status = 0;
		strcpy(xInfoP->name, volname);
		xInfoP->volid = currVolID;
		Log("1 Volser: XListOneVolume: Volume %u needs to be salvaged\n", currVolID);
		goto drop;
	    }

	    /*
	     * Pull out the desired info and stuff it into the area we'll be
	     * returning to our caller.
	     */
	    strcpy(xInfoP->name, volDiskDataP->name);
	    xInfoP->volid = volDiskDataP->id;
	    xInfoP->type = volDiskDataP->type;
	    xInfoP->backupID = volDiskDataP->backupId;
	    xInfoP->parentID = volDiskDataP->parentId;
	    xInfoP->cloneID = volDiskDataP->cloneId;
	    xInfoP->status = VOK;
	    xInfoP->copyDate = volDiskDataP->copyDate;
	    xInfoP->inUse = volDiskDataP->inUse;
	    xInfoP->creationDate = volDiskDataP->creationDate;
	    xInfoP->accessDate = volDiskDataP->accessDate;
	    xInfoP->updateDate = volDiskDataP->updateDate;
	    xInfoP->backupDate = volDiskDataP->backupDate;
	    xInfoP->filecount = volDiskDataP->filecount;
	    xInfoP->maxquota = volDiskDataP->maxquota;
	    xInfoP->size = volDiskDataP->diskused;

	    /*
	     * Copy out the stat fields in a single operation.
	     */
	    now = FT_ApproxTime();
	    if (now - volDiskDataP->dayUseDate > OneDay) {
		xInfoP->dayUse = 0;
		memset((char *)&(xInfoP->stat_reads[0]), 0, numStatBytes);
	    } else {
		xInfoP->dayUse = volDiskDataP->dayUse;
		memcpy((char *)&(xInfoP->stat_reads[0]),
		   (char *)&(volDiskDataP->stat_reads[0]), numStatBytes);
	    }

	    /*
	     * We're done copying.  Detach the volume and iterate (at this
	     * point, since we found our volume, we'll then drop out of the
	     * loop).
	     */
	    VDetachVolume(&error, tv);
	    tv = (Volume *) 0;
	    if (error) {
		xInfoP->status = 0;
		strcpy(xInfoP->name, volname);
		Log("1 Volser: XListOneVolumes Couldn't detach volume %s\n",
		    volname);
		goto drop;
	    }

	    /*
	     * At this point, we're golden.
	     */
	    code = 0;
	}			/*Found desired volume */
	GetNextVol(dirp, volname, &currVolID);
    }

    /*
     * Drop the transaction we have for this volume.
     */
  drop:
    if (tv) {
	VDetachVolume(&error, tv);
	tv = (Volume *) 0;
    }
    if (ttc) {
	DeleteTrans(ttc, 1);
	ttc = (struct volser_trans *)0;
    }

    /*
     * Clean up before going to dinner: close the partition directory,
     * return the proper value.
     */
    closedir(dirp);
    return (code);

}				/*SAFSVolXListOneVolume */

/*returns all the volumes on partition partid. If flags = 1 then all the 
* relevant info about the volumes  is also returned */
afs_int32
SAFSVolListVolumes(struct rx_call *acid, afs_int32 partid, afs_int32 flags, 
		   volEntries *volumeInfo)
{
    afs_int32 code;

    code = VolListVolumes(acid, partid, flags, volumeInfo);
    osi_auditU(acid, VS_ListVolEvent, code, AUD_END);
    return code;
}

afs_int32
VolListVolumes(struct rx_call *acid, afs_int32 partid, afs_int32 flags, 
		   volEntries *volumeInfo)
{
    volintInfo *pntr;
    register struct Volume *tv;
    struct DiskPartition *partP;
    struct volser_trans *ttc;
    afs_int32 allocSize = 1000;	/*to be changed to a larger figure */
    char pname[9], volname[20];
    afs_int32 error = 0;
    DIR *dirp;
    afs_int32 volid;
    unsigned int now;

    volumeInfo->volEntries_val =
	(volintInfo *) malloc(allocSize * sizeof(volintInfo));
    pntr = volumeInfo->volEntries_val;
    volumeInfo->volEntries_len = 0;
    if (GetPartName(partid, pname))
	return VOLSERILLEGAL_PARTITION;
    if (!(partP = VGetPartition(pname, 0)))
	return VOLSERILLEGAL_PARTITION;
    dirp = opendir(VPartitionPath(partP));
    if (dirp == NULL)
	return VOLSERILLEGAL_PARTITION;
    strcpy(volname, "");
    while (strcmp(volname, "EOD")) {	/*while there are more partitions in the partition */
	ttc = (struct volser_trans *)0;	/* new one for each pass */
	tv = (Volume *) 0;	/* volume not attached */

	if (!strcmp(volname, "")) {	/* its not a volume, fetch next file */
	    GetNextVol(dirp, volname, &volid);
	    continue;		/*back to while loop */
	}

	if (flags) {		/*copy other things too */
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();	/*make sure that the client doesnot time out */
#endif
	    ttc = NewTrans(volid, partid);
	    if (!ttc) {
		pntr->status = VBUSY;
		pntr->volid = volid;
		goto drop;
	    }
	    tv = VAttachVolumeByName(&error, pname, volname, V_PEEK);
	    if (error) {
		pntr->status = 0;	/*things are messed up */
		strcpy(pntr->name, volname);
		pntr->volid = volid;
		Log("1 Volser: ListVolumes: Could not attach volume %u (%s) error=%d\n", volid, volname, error);
		goto drop;
	    }
	    if (tv->header->diskstuff.needsSalvaged) {
		/*this volume will be salvaged */
		pntr->status = 0;
		strcpy(pntr->name, volname);
		pntr->volid = volid;
		Log("1 Volser: ListVolumes: Volume %u (%s) needs to be salvaged\n", volid, volname);
		goto drop;
	    }

	    if (tv->header->diskstuff.destroyMe == DESTROY_ME) {
		/*this volume will be salvaged */
		goto drop2;
	    }
	    /*read in the relevant info */
	    pntr->status = VOK;	/*its ok */
	    pntr->volid = tv->header->diskstuff.id;
	    strcpy(pntr->name, tv->header->diskstuff.name);
	    pntr->type = tv->header->diskstuff.type;	/*if ro volume */
	    pntr->cloneID = tv->header->diskstuff.cloneId;	/*if rw volume */
	    pntr->backupID = tv->header->diskstuff.backupId;
	    pntr->parentID = tv->header->diskstuff.parentId;
	    pntr->copyDate = tv->header->diskstuff.copyDate;
	    pntr->inUse = tv->header->diskstuff.inUse;
	    pntr->size = tv->header->diskstuff.diskused;
	    pntr->needsSalvaged = tv->header->diskstuff.needsSalvaged;
	    pntr->maxquota = tv->header->diskstuff.maxquota;
	    pntr->filecount = tv->header->diskstuff.filecount;
	    now = FT_ApproxTime();
	    if (now - tv->header->diskstuff.dayUseDate > OneDay)
		pntr->dayUse = 0;
	    else
		pntr->dayUse = tv->header->diskstuff.dayUse;
	    pntr->creationDate = tv->header->diskstuff.creationDate;
	    pntr->accessDate = tv->header->diskstuff.accessDate;
	    pntr->updateDate = tv->header->diskstuff.updateDate;
	    pntr->backupDate = tv->header->diskstuff.backupDate;
	    pntr->spare0 = tv->header->diskstuff.minquota;
	    pntr->spare1 =
		(long)tv->header->diskstuff.weekUse[0] +
		(long)tv->header->diskstuff.weekUse[1] +
		(long)tv->header->diskstuff.weekUse[2] +
		(long)tv->header->diskstuff.weekUse[3] +
		(long)tv->header->diskstuff.weekUse[4] +
		(long)tv->header->diskstuff.weekUse[5] +
		(long)tv->header->diskstuff.weekUse[6];
	    pntr->spare2 = V_volUpCounter(tv);
	    pntr->flags = pntr->spare3 = (long)0;
	    VDetachVolume(&error, tv);	/*free the volume */
	    tv = (Volume *) 0;
	    if (error) {
		pntr->status = 0;	/*things are messed up */
		strcpy(pntr->name, volname);
		Log("1 Volser: ListVolumes: Could not detach volume %s\n",
		    volname);
		goto drop;
	    }
	} else {
	    pntr->volid = volid;
	    /*just volids are needed */
	}

      drop:
	if (ttc) {
	    DeleteTrans(ttc, 1);
	    ttc = (struct volser_trans *)0;
	}
	pntr++;
	volumeInfo->volEntries_len += 1;
	if ((allocSize - volumeInfo->volEntries_len) < 5) {
	    /*running out of space, allocate more space */
	    allocSize = (allocSize * 3) / 2;
	    pntr =
		(volintInfo *) realloc((char *)volumeInfo->volEntries_val,
				       allocSize * sizeof(volintInfo));
	    if (pntr == NULL) {
		if (tv) {
		    VDetachVolume(&error, tv);
		    tv = (Volume *) 0;
		}
		if (ttc) {
		    DeleteTrans(ttc, 1);
		    ttc = (struct volser_trans *)0;
		}
		closedir(dirp);
		return VOLSERNO_MEMORY;
	    }
	    volumeInfo->volEntries_val = pntr;	/* point to new block */
	    /* set pntr to the right position */
	    pntr = volumeInfo->volEntries_val + volumeInfo->volEntries_len;

	}

      drop2:
	if (tv) {
	    VDetachVolume(&error, tv);
	    tv = (Volume *) 0;
	}
	if (ttc) {
	    DeleteTrans(ttc, 1);
	    ttc = (struct volser_trans *)0;
	}
	GetNextVol(dirp, volname, &volid);

    }
    closedir(dirp);
    if (ttc)
	DeleteTrans(ttc, 1);

    return 0;
}

/*------------------------------------------------------------------------
 * EXPORTED SAFSVolXListVolumes
 *
 * Description:
 *	Returns all the volumes on partition a_partID.  If a_flags
 *	is set to 1, then all the relevant extended volume information
 *	is also returned.
 *
 * Arguments:
 *	a_rxCidP       : Pointer to the Rx call we're performing.
 *	a_partID       : Partition for which we want the extended list.
 *	a_flags        : Various flags.
 *	a_volumeXInfoP : Ptr to the extended info blob.
 *
 * Returns:
 *	0			Successful operation
 *	VOLSERILLEGAL_PARTITION if we got a bogus partition ID
 *	VOLSERNO_MEMORY         if we ran out of memory allocating
 *				our return blob
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

afs_int32
SAFSVolXListVolumes(struct rx_call *a_rxCidP, afs_int32 a_partID,
		    afs_int32 a_flags, volXEntries *a_volumeXInfoP)
{
    afs_int32 code;

    code = VolXListVolumes(a_rxCidP, a_partID, a_flags, a_volumeXInfoP);
    osi_auditU(a_rxCidP, VS_XLstVolEvent, code, AUD_END);
    return code;
}

afs_int32
VolXListVolumes(struct rx_call *a_rxCidP, afs_int32 a_partID,
		    afs_int32 a_flags, volXEntries *a_volumeXInfoP)
{				/*SAFSVolXListVolumes */

    volintXInfo *xInfoP;	/*Ptr to the extended vol info */
    register struct Volume *tv;	/*Volume ptr */
    struct DiskPartition *partP;	/*Ptr to partition */
    struct volser_trans *ttc;	/*Volume transaction ptr */
    afs_int32 allocSize = 1000;	/*To be changed to a larger figure */
    char pname[9], volname[20];	/*Partition, volume names */
    afs_int32 error = 0;	/*Return code */
    DIR *dirp;			/*Partition directory ptr */
    afs_int32 volid;		/*Current volume ID */
    struct VolumeDiskData *volDiskDataP;	/*Ptr to on-disk volume data */
    int numStatBytes;		/*Num stat bytes to copy per volume */
    unsigned int now;

    /*
     * Allocate a large array of extended volume info structures, then
     * set it up for action.
     */
    a_volumeXInfoP->volXEntries_val =
	(volintXInfo *) malloc(allocSize * sizeof(volintXInfo));
    xInfoP = a_volumeXInfoP->volXEntries_val;
    a_volumeXInfoP->volXEntries_len = 0;

    /*
     * If the partition name we've been given is bad, bogue out.
     */
    if (GetPartName(a_partID, pname))
	return (VOLSERILLEGAL_PARTITION);

    /*
     * Open the directory representing the given AFS parttion.  If we can't
     * do that, we lose.
     */
    if (!(partP = VGetPartition(pname, 0)))
	return VOLSERILLEGAL_PARTITION;
    dirp = opendir(VPartitionPath(partP));
    if (dirp == NULL)
	return (VOLSERILLEGAL_PARTITION);

    /*
     * Sweep through the partition directory, acting on each entry.  First,
     * of course, figure out how many stat bytes to copy out of each volume.
     */
    numStatBytes =
	4 * ((2 * VOLINT_STATS_NUM_RWINFO_FIELDS) +
	     (4 * VOLINT_STATS_NUM_TIME_FIELDS));
    strcpy(volname, "");
    while (strcmp(volname, "EOD")) {
	ttc = (struct volser_trans *)0;	/*New one for each pass */
	tv = (Volume *) 0;	/*Volume not yet attached */

	/*
	 * If this is not a volume, move on to the next entry in the
	 * partition's directory.
	 */
	if (!strcmp(volname, "")) {
	    GetNextVol(dirp, volname, &volid);
	    continue;
	}

	if (a_flags) {
	    /*
	     * Full info about the volume desired.  Poll to make sure the
	     * client doesn't time out, then start up a new transaction.
	     */
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();
#endif
	    ttc = NewTrans(volid, a_partID);
	    if (!ttc) {
		/*
		 * Couldn't get a transaction on this volume; let our caller
		 * know it's busy.
		 */
		xInfoP->status = VBUSY;
		xInfoP->volid = volid;
		goto drop;
	    }

	    /*
	     * Attach the volume, give up on this volume if we can't.
	     */
	    tv = VAttachVolumeByName(&error, pname, volname, V_PEEK);
	    if (error) {
		xInfoP->status = 0;	/*things are messed up */
		strcpy(xInfoP->name, volname);
		xInfoP->volid = volid;
		Log("1 Volser: XListVolumes: Could not attach volume %u\n",
		    volid);
		goto drop;
	    }

	    /*
	     * Also bag out on this volume if it's been marked as needing a
	     * salvage or to-be-destroyed.
	     */
	    volDiskDataP = &(tv->header->diskstuff);
	    if (volDiskDataP->needsSalvaged) {
		xInfoP->status = 0;
		strcpy(xInfoP->name, volname);
		xInfoP->volid = volid;
		Log("1 Volser: XListVolumes: Volume %u needs to be salvaged\n", volid);
		goto drop;
	    }

	    if (volDiskDataP->destroyMe == DESTROY_ME)
		goto drop2;

	    /*
	     * Pull out the desired info and stuff it into the area we'll be
	     * returning to our caller.
	     */
	    strcpy(xInfoP->name, volDiskDataP->name);
	    xInfoP->volid = volDiskDataP->id;
	    xInfoP->type = volDiskDataP->type;
	    xInfoP->backupID = volDiskDataP->backupId;
	    xInfoP->parentID = volDiskDataP->parentId;
	    xInfoP->cloneID = volDiskDataP->cloneId;
	    xInfoP->status = VOK;
	    xInfoP->copyDate = volDiskDataP->copyDate;
	    xInfoP->inUse = volDiskDataP->inUse;
	    xInfoP->creationDate = volDiskDataP->creationDate;
	    xInfoP->accessDate = volDiskDataP->accessDate;
	    xInfoP->updateDate = volDiskDataP->updateDate;
	    xInfoP->backupDate = volDiskDataP->backupDate;
	    now = FT_ApproxTime();
	    if (now - volDiskDataP->dayUseDate > OneDay)
		xInfoP->dayUse = 0;
	    else
		xInfoP->dayUse = volDiskDataP->dayUse;
	    xInfoP->filecount = volDiskDataP->filecount;
	    xInfoP->maxquota = volDiskDataP->maxquota;
	    xInfoP->size = volDiskDataP->diskused;

	    /*
	     * Copy out the stat fields in a single operation.
	     */
	    memcpy((char *)&(xInfoP->stat_reads[0]),
		   (char *)&(volDiskDataP->stat_reads[0]), numStatBytes);

	    /*
	     * We're done copying.  Detach the volume and iterate.
	     */
	    VDetachVolume(&error, tv);
	    tv = (Volume *) 0;
	    if (error) {
		xInfoP->status = 0;
		strcpy(xInfoP->name, volname);
		Log("1 Volser: XListVolumes: Could not detach volume %s\n",
		    volname);
		goto drop;
	    }
	} /*Full contents desired */
	else
	    /*
	     * Just volume IDs are needed.
	     */
	    xInfoP->volid = volid;

      drop:
	/*
	 * Drop the transaction we have for this volume.
	 */
	if (ttc) {
	    DeleteTrans(ttc, 1);
	    ttc = (struct volser_trans *)0;
	}

	/*
	 * Bump the pointer in the data area we're building, along with
	 * the count of the number of entries it contains.
	 */
	xInfoP++;
	(a_volumeXInfoP->volXEntries_len)++;
	if ((allocSize - a_volumeXInfoP->volXEntries_len) < 5) {
	    /*
	     * We're running out of space in the area we've built.  Grow it.
	     */
	    allocSize = (allocSize * 3) / 2;
	    xInfoP = (volintXInfo *)
		realloc((char *)a_volumeXInfoP->volXEntries_val,
			(allocSize * sizeof(volintXInfo)));
	    if (xInfoP == NULL) {
		/*
		 * Bummer, no memory. Bag it, tell our caller what went wrong.
		 */
		if (tv) {
		    VDetachVolume(&error, tv);
		    tv = (Volume *) 0;
		}
		if (ttc) {
		    DeleteTrans(ttc, 1);
		    ttc = (struct volser_trans *)0;
		}
		closedir(dirp);
		return (VOLSERNO_MEMORY);
	    }

	    /*
	     * Memory reallocation worked.  Correct our pointers so they
	     * now point to the new block and the current open position within
	     * the new block.
	     */
	    a_volumeXInfoP->volXEntries_val = xInfoP;
	    xInfoP =
		a_volumeXInfoP->volXEntries_val +
		a_volumeXInfoP->volXEntries_len;
	}
	/*Need more space */
      drop2:
	/*
	 * Detach our current volume and the transaction on it, then move on
	 * to the next volume in the partition directory.
	 */
	if (tv) {
	    VDetachVolume(&error, tv);
	    tv = (Volume *) 0;
	}
	if (ttc) {
	    DeleteTrans(ttc, 1);
	    ttc = (struct volser_trans *)0;
	}
	GetNextVol(dirp, volname, &volid);
    }				/*Sweep through the partition directory */

    /*
     * We've examined all entries in the partition directory.  Close it,
     * delete our transaction (if any), and go home happy.
     */
    closedir(dirp);
    if (ttc)
	DeleteTrans(ttc, 1);
    return (0);

}				/*SAFSVolXListVolumes */

/*this call is used to monitor the status of volser for debugging purposes.
 *information about all the active transactions is returned in transInfo*/
afs_int32
SAFSVolMonitor(struct rx_call *acid, transDebugEntries *transInfo)
{
    afs_int32 code;

    code = VolMonitor(acid, transInfo);
    osi_auditU(acid, VS_MonitorEvent, code, AUD_END);
    return code;
}

afs_int32
VolMonitor(struct rx_call *acid, transDebugEntries *transInfo)
{
    transDebugInfo *pntr;
    afs_int32 allocSize = 50;
    struct volser_trans *tt, *allTrans;

    transInfo->transDebugEntries_val =
	(transDebugInfo *) malloc(allocSize * sizeof(transDebugInfo));
    pntr = transInfo->transDebugEntries_val;
    transInfo->transDebugEntries_len = 0;
    allTrans = TransList();
    if (allTrans == (struct volser_trans *)0)
	return 0;		/*no active transactions */
    for (tt = allTrans; tt; tt = tt->next) {	/*copy relevant info into pntr */
	pntr->tid = tt->tid;
	pntr->time = tt->time;
	pntr->creationTime = tt->creationTime;
	pntr->returnCode = tt->returnCode;
	pntr->volid = tt->volid;
	pntr->partition = tt->partition;
	pntr->iflags = tt->iflags;
	pntr->vflags = tt->vflags;
	pntr->tflags = tt->tflags;
	strcpy(pntr->lastProcName, tt->lastProcName);
	pntr->callValid = 0;
	if (tt->rxCallPtr) {	/*record call related info */
	    pntr->callValid = 1;
	    pntr->readNext = tt->rxCallPtr->rnext;
	    pntr->transmitNext = tt->rxCallPtr->tnext;
	    pntr->lastSendTime = tt->rxCallPtr->lastSendTime;
	    pntr->lastReceiveTime = tt->rxCallPtr->lastReceiveTime;
	}
	pntr++;
	transInfo->transDebugEntries_len += 1;
	if ((allocSize - transInfo->transDebugEntries_len) < 5) {	/*alloc some more space */
	    allocSize = (allocSize * 3) / 2;
	    pntr =
		(transDebugInfo *) realloc((char *)transInfo->
					   transDebugEntries_val,
					   allocSize *
					   sizeof(transDebugInfo));
	    transInfo->transDebugEntries_val = pntr;
	    pntr =
		transInfo->transDebugEntries_val +
		transInfo->transDebugEntries_len;
	    /*set pntr to right position */
	}

    }

    return 0;
}

afs_int32
SAFSVolSetIdsTypes(struct rx_call *acid, afs_int32 atid, char name[], afs_int32 type, afs_int32 pId, afs_int32 cloneId, afs_int32 backupId)
{
    afs_int32 code;

    code = VolSetIdsTypes(acid, atid, name, type, pId, cloneId, backupId);
    osi_auditU(acid, VS_SetIdTyEvent, code, AUD_LONG, atid, AUD_STR, name,
	       AUD_LONG, type, AUD_LONG, pId, AUD_LONG, cloneId, AUD_LONG,
	       backupId, AUD_END);
    return code;
}

afs_int32
VolSetIdsTypes(struct rx_call *acid, afs_int32 atid, char name[], afs_int32 type, afs_int32 pId, afs_int32 cloneId, afs_int32 backupId)
{
    struct Volume *tv;
    afs_int32 error = 0;
    register struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];

    if (strlen(name) > 31)
	return VOLSERBADNAME;
    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    /* find the trans */
    tt = FindTrans(atid);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolSetIds: volume %u has been deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "SetIdsTypes");
    tt->rxCallPtr = acid;
    tv = tt->volume;

    V_type(tv) = type;
    V_backupId(tv) = backupId;
    V_cloneId(tv) = cloneId;
    V_parentId(tv) = pId;
    strcpy((&V_disk(tv))->name, name);
    VUpdateVolume(&error, tv);
    if (error) {
	Log("1 Volser: SetIdsTypes: VUpdate failed code %d\n", error);
	LogError(error);
	goto fail;
    }
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt) && !error)
	return VOLSERTRELE_ERROR;

    return error;
  fail:
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt) && !error)
	return VOLSERTRELE_ERROR;
    return error;
}

afs_int32
SAFSVolSetDate(struct rx_call *acid, afs_int32 atid, afs_int32 cdate)
{
    afs_int32 code;

    code = VolSetDate(acid, atid, cdate);
    osi_auditU(acid, VS_SetDateEvent, code, AUD_LONG, atid, AUD_LONG, cdate,
	       AUD_END);
    return code;
}

afs_int32
VolSetDate(struct rx_call *acid, afs_int32 atid, afs_int32 cdate)
{
    struct Volume *tv;
    afs_int32 error = 0;
    register struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    /* find the trans */
    tt = FindTrans(atid);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolSetDate: volume %u has been deleted \n", tt->volid);
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "SetDate");
    tt->rxCallPtr = acid;
    tv = tt->volume;

    V_creationDate(tv) = cdate;
    VUpdateVolume(&error, tv);
    if (error) {
	Log("1 Volser: SetDate: VUpdate failed code %d\n", error);
	LogError(error);
	goto fail;
    }
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt) && !error)
	return VOLSERTRELE_ERROR;

    return error;
  fail:
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt) && !error)
	return VOLSERTRELE_ERROR;
    return error;
}

#ifdef AFS_NAMEI_ENV
/* 
 * Inode number format  (from namei_ops.c): 
 * low 26 bits - vnode number - all 1's if volume special file.
 * next 3 bits - tag
 * next 3 bits spare (0's)
 * high 32 bits - uniquifier (regular) or type if spare
 */
#define NAMEI_VNODEMASK    0x003ffffff
#define NAMEI_TAGMASK      0x7
#define NAMEI_TAGSHIFT     26
#define NAMEI_UNIQMASK     0xffffffff
#define NAMEI_UNIQSHIFT    32
#define NAMEI_INODESPECIAL ((Inode)NAMEI_VNODEMASK)
#define NAMEI_VNODESPECIAL NAMEI_VNODEMASK
#endif /* AFS_NAMEI_ENV */

afs_int32
SAFSVolConvertROtoRWvolume(struct rx_call *acid, afs_int32 partId,
			   afs_int32 volumeId)
{
#if defined(AFS_NAMEI_ENV) && !defined(AFS_NT40_ENV)
    DIR *dirp;
    char pname[16];
    char volname[20];
    afs_int32 error = 0;
    afs_int32 volid;
    int found = 0;
    char caller[MAXKTCNAMELEN];
    char headername[16];
    char opath[256];
    char npath[256];
    struct VolumeDiskHeader h;
    int fd;
    IHandle_t *ih;
    Inode ino;
    struct DiskPartition *dp;

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    if (GetPartName(partId, pname))
	return VOLSERILLEGAL_PARTITION;
    dirp = opendir(pname);
    if (dirp == NULL)
	return VOLSERILLEGAL_PARTITION;
    strcpy(volname, "");

    while (strcmp(volname, "EOD") && !found) {	/*while there are more volumes in the partition */
	GetNextVol(dirp, volname, &volid);
	if (strcmp(volname, "")) {	/* its a volume */
	    if (volid == volumeId)
		found = 1;
	}
    }
    if (!found)
	return ENOENT;
    (void)afs_snprintf(headername, sizeof headername, VFORMAT, volumeId);
    (void)afs_snprintf(opath, sizeof opath, "%s/%s", pname, headername);
    fd = open(opath, O_RDONLY);
    if (fd < 0) {
	Log("1 SAFS_VolConvertROtoRWvolume: Couldn't open header for RO-volume %lu.\n", volumeId);
	return ENOENT;
    }
    if (read(fd, &h, sizeof(h)) != sizeof(h)) {
	Log("1 SAFS_VolConvertROtoRWvolume: Couldn't read header for RO-volume %lu.\n", volumeId);
	close(fd);
	return EIO;
    }
    close(fd);
    FSYNC_VolOp(volumeId, pname, FSYNC_VOL_BREAKCBKS, 0, NULL);

    for (dp = DiskPartitionList; dp && strcmp(dp->name, pname);
	 dp = dp->next);
    if (!dp) {
	Log("1 SAFS_VolConvertROtoRWvolume: Couldn't find DiskPartition for %s\n", pname);
	return EIO;
    }
    ino = namei_MakeSpecIno(h.parent, VI_LINKTABLE);
    IH_INIT(ih, dp->device, h.parent, ino);

    error = namei_ConvertROtoRWvolume(ih, volumeId);
    if (error)
	return error;
    h.id = h.parent;
    h.volumeInfo_hi = h.id;
    h.smallVnodeIndex_hi = h.id;
    h.largeVnodeIndex_hi = h.id;
    h.linkTable_hi = h.id;
    (void)afs_snprintf(headername, sizeof headername, VFORMAT, h.id);
    (void)afs_snprintf(npath, sizeof npath, "%s/%s", pname, headername);
    fd = open(npath, O_CREAT | O_EXCL | O_RDWR, 0644);
    if (fd < 0) {
	Log("1 SAFS_VolConvertROtoRWvolume: Couldn't create header for RW-volume %lu.\n", h.id);
	return EIO;
    }
    if (write(fd, &h, sizeof(h)) != sizeof(h)) {
	Log("1 SAFS_VolConvertROtoRWvolume: Couldn't write header for RW-volume %lu.\n", h.id);
	close(fd);
	return EIO;
    }
    close(fd);
    if (unlink(opath) < 0) {
	Log("1 SAFS_VolConvertROtoRWvolume: Couldn't unlink RO header, error = %d\n", error);
    }
    FSYNC_VolOp(volumeId, pname, FSYNC_VOL_DONE, 0, NULL);
    FSYNC_VolOp(h.id, pname, FSYNC_VOL_ON, 0, NULL);
    return 0;
#else /* AFS_NAMEI_ENV */
    return EINVAL;
#endif /* AFS_NAMEI_ENV */
}

afs_int32
SAFSVolGetSize(struct rx_call *acid, afs_int32 fromTrans, afs_int32 fromDate,
	       register struct volintSize *size)
{
    int code = 0;
    register struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(fromTrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	TRELE(tt);
	return ENOENT;
    }
    strcpy(tt->lastProcName, "GetSize");
    tt->rxCallPtr = acid;
    code = SizeDumpVolume(acid, tt->volume, fromDate, 1, size);	/* measure volume's data */
    tt->rxCallPtr = (struct rx_call *)0;
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

/*    osi_auditU(acid, VS_DumpEvent, code, AUD_LONG, fromTrans, AUD_END);  */
    return code;
}

/* GetPartName - map partid (a decimal number) into pname (a string)
 * Since for NT we actually want to return the drive name, we map through the
 * partition struct.
 */
static int
GetPartName(afs_int32 partid, char *pname)
{
    if (partid < 0)
	return -1;
    if (partid < 26) {
	strcpy(pname, "/vicep");
	pname[6] = 'a' + partid;
	pname[7] = '\0';
	return 0;
    } else if (partid < VOLMAXPARTS) {
	strcpy(pname, "/vicep");
	partid -= 26;
	pname[6] = 'a' + (partid / 26);
	pname[7] = 'a' + (partid % 26);
	pname[8] = '\0';
	return 0;
    } else
	return -1;
}
