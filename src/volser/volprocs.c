/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2007-2008 Sine Nomine Associates
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/opr.h>
#ifdef AFS_PTHREAD_ENV
# include <opr/lock.h>
#endif

#include <rx/rx.h>
#include <rx/rxkad.h>
#include <rx/rx_queue.h>
#include <afs/afsint.h>
#include <afs/prs_fs.h>
#include <afs/nfs.h>
#include <lwp.h>
#include <afs/afs_lock.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/ihandle.h>
#ifdef AFS_NT40_ENV
#include <afs/ntops.h>
#endif
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/volume_inline.h>
#include <afs/partition.h>
#include "vol.h"
#include <afs/daemon_com.h>
#include <afs/fssync.h>
#include <afs/acl.h>
#include "afs/audit.h"
#include <afs/dir.h>
#include <afs/afsutil.h>
#include <afs/com_err.h>
#include <afs/vol_prototypes.h>
#include <afs/errors.h>

#include "volser.h"
#include "voltrans_inline.h"
#include "volint.h"

#include "volser_internal.h"
#include "physio.h"
#include "dumpstuff.h"

extern int DoLogging;
extern struct afsconf_dir *tdir;
extern int DoPreserveVolumeStats;
extern int restrictedQueryLevel;
extern enum vol_s2s_crypt doCrypt;

extern void LogError(afs_int32 errcode);

/* Forward declarations */
static int GetPartName(afs_int32 partid, char *pname);

#define OneDay (24*60*60)

#ifdef AFS_NT40_ENV
#define ENOTCONN 134
#endif

afs_int32 localTid = 1;

static afs_int32 VolPartitionInfo(struct rx_call *, char *pname,
				  struct diskPartition64 *);
static afs_int32 VolNukeVolume(struct rx_call *, afs_int32, afs_uint32);
static afs_int32 VolCreateVolume(struct rx_call *, afs_int32, char *,
				 afs_int32, afs_uint32, afs_uint32 *,
				 afs_int32 *);
static afs_int32 VolDeleteVolume(struct rx_call *, afs_int32);
static afs_int32 VolClone(struct rx_call *, afs_int32, VolumeId,
			  afs_int32, char *, VolumeId *);
static afs_int32 VolReClone(struct rx_call *, afs_int32, VolumeId);
static afs_int32 VolTransCreate(struct rx_call *, VolumeId, afs_int32,
				afs_int32, afs_int32 *);
static afs_int32 VolGetNthVolume(struct rx_call *, afs_int32, afs_uint32 *,
				 afs_int32 *);
static afs_int32 VolGetFlags(struct rx_call *, afs_int32, afs_int32 *);
static afs_int32 VolSetFlags(struct rx_call *, afs_int32, afs_int32 );
static afs_int32 VolForward(struct rx_call *, afs_int32, afs_int32,
			    struct destServer *destination, afs_int32,
			    struct restoreCookie *cookie);
static afs_int32 VolDump(struct rx_call *, afs_int32, afs_int32, afs_int32);
static afs_int32 VolRestore(struct rx_call *, afs_int32, struct restoreCookie *);
static afs_int32 VolEndTrans(struct rx_call *, afs_int32, afs_int32 *);
static afs_int32 VolSetForwarding(struct rx_call *, afs_int32, afs_int32);
static afs_int32 VolGetStatus(struct rx_call *, afs_int32,
			      struct volser_status *);
static afs_int32 VolSetInfo(struct rx_call *, afs_int32, struct volintInfo *);
static afs_int32 VolGetName(struct rx_call *, afs_int32, char **);
static afs_int32 VolListPartitions(struct rx_call *, struct pIDs *);
static afs_int32 XVolListPartitions(struct rx_call *, struct partEntries *);
static afs_int32 VolListOneVolume(struct rx_call *, afs_int32, VolumeId,
				  volEntries *);
static afs_int32 VolXListOneVolume(struct rx_call *, afs_int32, VolumeId,
				   volXEntries *);
static afs_int32 VolListVolumes(struct rx_call *, afs_int32, afs_int32,
				volEntries *);
static afs_int32 VolXListVolumes(struct rx_call *, afs_int32, afs_int32,
				volXEntries *);
static afs_int32 VolMonitor(struct rx_call *, transDebugEntries *);
static afs_int32 VolSetIdsTypes(struct rx_call *, afs_int32, char [],
				afs_int32, VolumeId, VolumeId,
				VolumeId);
static afs_int32 VolSetDate(struct rx_call *, afs_int32, afs_int32);

/**
 * Return the host address of the caller as a string.
 *
 * @param[in]  acid    incoming rx call
 * @param[out] buffer  buffer to be filled with the addess string
 *
 * @return address as formatted by inet_ntoa
 */
static_inline char *
callerAddress(struct rx_call *acid, char *buffer)
{
    afs_uint32 ip = rx_HostOf(rx_PeerOf(rx_ConnectionOf(acid)));
    return afs_inet_ntoa_r(ip, buffer);
}

/* this call unlocks all of the partition locks we've set */
int
VPFullUnlock_r(void)
{
    struct DiskPartition64 *tp;
    for (tp = DiskPartitionList; tp; tp = tp->next) {
	if (tp->lock_fd != INVALID_FD) {
            OS_CLOSE(tp->lock_fd);
	    tp->lock_fd = INVALID_FD;
	}
    }
    return 0;
}

int
VPFullUnlock(void)
{
    int code;
    VOL_LOCK;
    code = VPFullUnlock_r();
    VOL_UNLOCK;
    return code;
}

static int
ConvertVolume(VolumeId avol, char *aname, afs_int32 asize)
{
    if (asize < 18)
	return -1;
    /* It's better using the Generic VFORMAT since otherwise we have to make changes to too many places... The 14 char limitation in names hits us again in AIX; print in field of 9 digits (still 10 for the rest), right justified with 0 padding */
    snprintf(aname, asize, VFORMAT, afs_printable_VolumeId_lu(avol));
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

#ifdef AFS_DEMAND_ATTACH_FS
/* normally we should use the regular salvaging functions from the volume
 * package, but this is a special case where we have a volume ID, but no
 * volume structure to give the volume package */
static void
SalvageUnknownVolume(VolumeId volid, char *part)
{
    afs_int32 code;

    Log("Scheduling salvage for allegedly nonexistent volume %lu part %s\n",
        afs_printable_uint32_lu(volid), part);

    code = FSYNC_VolOp(volid, part, FSYNC_VOL_FORCE_ERROR,
                       FSYNC_SALVAGE, NULL);
    if (code) {
	Log("SalvageUnknownVolume: error %ld trying to salvage vol %lu part %s\n",
	    afs_printable_int32_ld(code), afs_printable_uint32_lu(volid),
	    part);
    }
}
#endif /* AFS_DEMAND_ATTACH_FS */

static struct Volume *
VAttachVolumeByName_retry(Error *ec, char *partition, char *name, int mode)
{
    struct Volume *vp;

    *ec = 0;
    vp = VAttachVolumeByName(ec, partition, name, mode);

#ifdef AFS_DEMAND_ATTACH_FS
    {
        int i;
        /*
         * The fileserver will take care of keeping track of how many
         * demand-salvages have been performed, and will force the volume to
         * ERROR if we've done too many. The limit on This loop is just a
         * failsafe to prevent trying to salvage forever. We want to attempt
         * attachment at least SALVAGE_COUNT_MAX times, since we want to
         * avoid prematurely exiting this loop, if we can.
         */
        for (i = 0; i < SALVAGE_COUNT_MAX*2 && *ec == VSALVAGING; i++) {
            sleep(SALVAGE_PRIO_UPDATE_INTERVAL);
            vp = VAttachVolumeByName(ec, partition, name, mode);
        }

        if (*ec == VSALVAGING) {
            *ec = VSALVAGE;
        }
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    return vp;
}

static struct Volume *
VAttachVolume_retry(Error *ec, afs_uint32 avolid, int amode)
{
    struct Volume *vp;

    *ec = 0;
    vp = VAttachVolume(ec, avolid, amode);

#ifdef AFS_DEMAND_ATTACH_FS
    {
        int i;
        /* see comment above in VAttachVolumeByName_retry */
        for (i = 0; i < SALVAGE_COUNT_MAX*2 && *ec == VSALVAGING; i++) {
            sleep(SALVAGE_PRIO_UPDATE_INTERVAL);
            vp = VAttachVolume(ec, avolid, amode);
        }

        if (*ec == VSALVAGING) {
            *ec = VSALVAGE;
        }
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    return vp;
}

/* the only attach function that takes a partition is "...ByName", so we use it */
static struct Volume *
XAttachVolume(afs_int32 *error, afs_uint32 avolid, afs_int32 apartid, int amode)
{
    char pbuf[30], vbuf[20];

    if (ConvertPartition(apartid, pbuf, sizeof(pbuf))) {
	*error = EINVAL;
	return NULL;
    }
    if (ConvertVolume(avolid, vbuf, sizeof(vbuf))) {
	*error = EINVAL;
	return NULL;
    }

    return VAttachVolumeByName_retry((Error *)error, pbuf, vbuf, amode);
}

/* Adapted from the file server; create a root directory for this volume */
static Error
ViceCreateRoot(Volume *vp)
{
    DirHandle dir;
    struct acl_accessList *ACL;
    AFSFid did;
    Inode inodeNumber, AFS_UNUSED nearInode;
    struct VnodeDiskObject *vnode;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[vLarge];
    IHandle_t *h;
    FdHandle_t *fdP;
    afs_fsize_t length;
    ssize_t nBytes;

    vnode = calloc(1, SIZEOF_LARGEDISKVNODE);
    if (!vnode)
	return ENOMEM;

    V_pref(vp, nearInode);
    inodeNumber =
	IH_CREATE(V_linkHandle(vp), V_device(vp),
		  VPartitionPath(V_partition(vp)), nearInode, V_parentId(vp),
		  1, 1, 0);
    if (!VALID_INO(inodeNumber)) {
	Log("ViceCreateRoot: IH_CREATE: %s\n", afs_error_message(errno));
	free(vnode);
	return EIO;
    }

    SetSalvageDirHandle(&dir, V_parentId(vp), vp->device, inodeNumber);
    did.Volume = V_id(vp);
    did.Vnode = (VnodeId) 1;
    did.Unique = 1;

    opr_Verify(!(afs_dir_MakeDir(&dir, (afs_int32 *)&did, (afs_int32 *)&did)));
    DFlush();			/* flush all modified dir buffers out */
    DZap(&dir);			/* Remove all buffers for this dir */
    length = afs_dir_Length(&dir);	/* Remember size of this directory */

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
    opr_Assert(fdP != NULL);
    nBytes = FDH_PWRITE(fdP, vnode, SIZEOF_LARGEDISKVNODE, vnodeIndexOffset(vcp, 1));
    opr_Assert(nBytes == SIZEOF_LARGEDISKVNODE);
    FDH_REALLYCLOSE(fdP);
    IH_RELEASE(h);
    VNDISK_GET_LEN(length, vnode);
    V_diskused(vp) = nBlocks(length);

    free(vnode);
    return 0;
}

afs_int32
SAFSVolPartitionInfo(struct rx_call *acid, char *pname, struct diskPartition
		     *partition)
{
    afs_int32 code;
    struct diskPartition64 *dp = malloc(sizeof(struct diskPartition64));

    memset(partition, 0, sizeof(*partition));
    code = VolPartitionInfo(acid, pname, dp);
    if (!code) {
	strncpy(partition->name, dp->name, 32);
	strncpy(partition->devName, dp->devName, 32);
	partition->lock_fd = dp->lock_fd;
	partition->free=RoundInt64ToInt32(dp->free);
	partition->minFree=RoundInt64ToInt32(dp->minFree);
    }
    free(dp);
    osi_auditU(acid, VS_ParInfEvent, code, AUD_STR, pname, AUD_END);
    return code;
}

afs_int32
SAFSVolPartitionInfo64(struct rx_call *acid, char *pname, struct diskPartition64
		     *partition)
{
    afs_int32 code;

    memset(partition, 0, sizeof(*partition));
    code = VolPartitionInfo(acid, pname, partition);
    osi_auditU(acid, VS_ParInfEvent, code, AUD_STR, pname, AUD_END);
    return code;
}

static afs_int32
VolPartitionInfo(struct rx_call *acid, char *pname, struct diskPartition64
		 *partition)
{
    struct DiskPartition64 *dp;

    if (!afsconf_CheckRestrictedQuery(tdir, acid, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

    VResetDiskUsage();
    dp = VGetPartition(pname, 0);
    if (dp) {
	strncpy(partition->name, dp->name, 32);
	strncpy(partition->devName, dp->devName, 32);
	partition->lock_fd = (int)dp->lock_fd;
	partition->free = dp->free;
	partition->minFree = dp->totalUsable;
	return 0;
    } else
	return VOLSERILLEGAL_PARTITION;
}

/* obliterate a volume completely, and slowly. */
afs_int32
SAFSVolNukeVolume(struct rx_call *acid, afs_int32 apartID, VolumeId avolID)
{
    afs_int32 code;

    code = VolNukeVolume(acid, apartID, avolID);
    osi_auditU(acid, VS_NukVolEvent, code, AUD_LONG, avolID, AUD_END);
    return code;
}

static afs_int32
VolNukeVolume(struct rx_call *acid, afs_int32 apartID, afs_uint32 avolID)
{
    char partName[50];
    afs_int32 error;
    Error verror;
    afs_int32 code;
    struct Volume *tvp;
    char caller[MAXKTCNAMELEN];

    /* check for access */
    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;
    if (DoLogging) {
	char buffer[16];
	Log("%s on %s is executing VolNukeVolume %u\n", caller,
	    callerAddress(acid, buffer), avolID);
    }

    if (volutil_PartitionName2_r(apartID, partName, sizeof(partName)) != 0)
	return VOLSERNOVOL;
    /* we first try to attach the volume in update mode, so that the file
     * server doesn't try to use it (and abort) while (or after) we delete it.
     * If we don't get the volume, that's fine, too.  We just won't put it back.
     */
    tvp = XAttachVolume(&error, avolID, apartID, V_VOLUPD);
    code = nuke(partName, avolID);
    if (tvp)
	VDetachVolume(&verror, tvp);
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
		    afs_int32 atype, VolumeId aparent, VolumeId *avolid,
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

static afs_int32
VolCreateVolume(struct rx_call *acid, afs_int32 apart, char *aname,
		    afs_int32 atype, afs_uint32 aparent, afs_uint32 *avolid,
		    afs_int32 *atrans)
{
    Error error;
    Volume *vp;
    Error junk;		/* discardable error code */
    afs_uint32 volumeID;
    afs_int32 doCreateRoot = 1;
    struct volser_trans *tt;
    char ppath[30];
    char caller[MAXKTCNAMELEN];

    if (strlen(aname) > 31)
	return VOLSERBADNAME;
    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;
    if (DoLogging) {
	char buffer[16];
	Log("%s on %s is executing CreateVolume '%s'\n", caller,
	    callerAddress(acid, buffer), aname);
    }
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
#ifdef AFS_DEMAND_ATTACH_FS
	if (error != VVOLEXISTS && error != EXDEV) {
	    SalvageUnknownVolume(volumeID, ppath);
	}
#endif
	Log("1 Volser: CreateVolume: Unable to create the volume; aborted, error code %u\n", error);
	LogError(error);
	DeleteTrans(tt, 1);
	return EIO;
    }
    V_uniquifier(vp) = 1;
    V_updateDate(vp) = V_creationDate(vp) = V_copyDate(vp);
    V_inService(vp) = V_blessed(vp) = 1;
    V_type(vp) = atype;
    AssignVolumeName(&V_disk(vp), aname, 0);
    if (doCreateRoot) {
	error = ViceCreateRoot(vp);
	if (error) {
	    Log("1 Volser: CreateVolume: Unable to create volume root dir; "
	        "error code %u\n", (unsigned)error);
	    DeleteTrans(tt, 1);
	    V_needsSalvaged(vp) = 1;
	    VDetachVolume(&junk, vp);
	    return EIO;
	}
    }
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
    VTRANS_OBJ_LOCK(tt);
    tt->volume = vp;
    *atrans = tt->tid;
    TSetRxCall_r(tt, acid, "CreateVolume");
    VTRANS_OBJ_UNLOCK(tt);
    Log("1 Volser: CreateVolume: volume %u (%s) created\n", volumeID, aname);
    TClearRxCall(tt);
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

static afs_int32
VolDeleteVolume(struct rx_call *acid, afs_int32 atrans)
{
    struct volser_trans *tt;
    Error error;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;
    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: Delete: volume %" AFS_VOLID_FMT " already deleted \n",
	    afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    if (DoLogging) {
	char buffer[16];
	Log("%s on %s is executing Delete Volume %" AFS_VOLID_FMT "\n", caller,
	    callerAddress(acid, buffer), afs_printable_VolumeId_lu(tt->volid));
    }
    TSetRxCall(tt, acid, "DeleteVolume");
    VPurgeVolume(&error, tt->volume);	/* don't check error code, it is not set! */
    V_destroyMe(tt->volume) = DESTROY_ME;
    if (tt->volume->needsPutBack) {
	tt->volume->needsPutBack = VOL_PUTBACK_DELETE; /* so endtrans does the right fssync opcode */
    }
    VTRANS_OBJ_LOCK(tt);
    tt->vflags |= VTDeleted;	/* so we know not to do anything else to it */
    TClearRxCall_r(tt);
    VTRANS_OBJ_UNLOCK(tt);
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    Log("1 Volser: Delete: volume %" AFS_VOLID_FMT " deleted \n",
	afs_printable_VolumeId_lu(tt->volid));
    return 0;			/* vpurgevolume doesn't set an error code */
}

/* make a clone of the volume associated with atrans, possibly giving it a new
 * number (allocate a new number if *newNumber==0, otherwise use *newNumber
 * for the clone's id).  The new clone is given the name newName.  Finally,
 * due to efficiency considerations, if purgeId is non-zero, we purge that
 * volume when doing the clone operation.  This may be useful when making
 * new backup volumes, for instance since the net result of a clone and a
 * purge generally leaves many inode ref counts the same, while doing them
 * separately would result in far more iincs and idecs being peformed
 * (and they are slow operations).
 */
/* for efficiency reasons, sometimes faster to piggyback a purge here */
afs_int32
SAFSVolClone(struct rx_call *acid, afs_int32 atrans, VolumeId purgeId,
	     afs_int32 newType, char *newName, VolumeId *newNumber)
{
    afs_int32 code;
    code = VolClone(acid, atrans, purgeId, newType, newName, newNumber);
    osi_auditU(acid, VS_CloneEvent, code, AUD_LONG, atrans, AUD_LONG, purgeId,
	       AUD_STR, newName, AUD_LONG, newType, AUD_LONG, *newNumber,
	       AUD_END);
    return code;
}

static afs_int32
VolClone(struct rx_call *acid, afs_int32 atrans, VolumeId purgeId,
	     afs_int32 newType, char *newName, VolumeId *newNumber)
{
    VolumeId newId;
    struct Volume *originalvp, *purgevp, *newvp;
    Error error, code;
    struct volser_trans *tt, *ttc;
    char caller[MAXKTCNAMELEN];
#ifdef AFS_DEMAND_ATTACH_FS
    struct Volume *salv_vp = NULL;
#endif

    if (strlen(newName) > 31)
	return VOLSERBADNAME;
    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    if (DoLogging) {
	char buffer[16];
	Log("%s on %s is executing Clone Volume new name=%s\n", caller,
	    callerAddress(acid, buffer), newName);
    }
    error = 0;
    purgevp = (Volume *) 0;
    newvp = (Volume *) 0;
    tt = ttc = (struct volser_trans *)0;

    if (!newNumber || !*newNumber) {
	Log("1 Volser: Clone: missing volume number for the clone; aborted\n");
	goto fail;
    }
    newId = *newNumber;

    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: Clone: volume %" AFS_VOLID_FMT " has been deleted \n", afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    ttc = NewTrans(newId, tt->partition);
    if (!ttc) {			/* someone is messing with the clone already */
	TRELE(tt);
	return VOLSERVOLBUSY;
    }
    TSetRxCall(tt, acid, "Clone");


    if (purgeId) {
	purgevp = VAttachVolume_retry(&error, purgeId, V_VOLUPD);
	if (error) {
	    Log("1 Volser: Clone: Could not attach 'purge' volume %" AFS_VOLID_FMT "; clone aborted\n", afs_printable_VolumeId_lu(purgeId));
	    goto fail;
	}
    } else {
	purgevp = NULL;
    }
    originalvp = tt->volume;
    if ((V_destroyMe(originalvp) == DESTROY_ME) || !V_inService(originalvp)) {
	Log("1 Volser: Clone: Volume %" AFS_VOLID_FMT " is offline and cannot be cloned\n",
	    afs_printable_VolumeId_lu(V_id(originalvp)));
	error = VOFFLINE;
	goto fail;
    }
    if (purgevp) {
	if (originalvp->device != purgevp->device) {
	    Log("1 Volser: Clone: Volumes %" AFS_VOLID_FMT " and %" AFS_VOLID_FMT " are on different devices\n", afs_printable_VolumeId_lu(tt->volid), afs_printable_VolumeId_lu(purgeId));
	    error = EXDEV;
	    goto fail;
	}
	if (V_type(purgevp) != readonlyVolume) {
	    Log("1 Volser: Clone: The \"purge\" volume must be a read only volume; aborted\n");
	    error = EINVAL;
	    goto fail;
	}
	if (V_parentId(originalvp) != V_parentId(purgevp)) {
	    Log("1 Volser: Clone: Volume %" AFS_VOLID_FMT " and volume %" AFS_VOLID_FMT " were not originally cloned from the same parent; aborted\n", afs_printable_VolumeId_lu(purgeId), afs_printable_VolumeId_lu(tt->volid));
	    error = EXDEV;
	    goto fail;
	}
    }

    error = 0;
#ifdef AFS_DEMAND_ATTACH_FS
    salv_vp = originalvp;
#endif

    if (purgeId == newId) {
	newvp = purgevp;
    } else {
	newvp =
	    VCreateVolume(&error, originalvp->partition->name, newId,
			  V_parentId(originalvp));
	if (error) {
	    Log("1 Volser: Clone: Couldn't create new volume %" AFS_VOLID_FMT " for parent %" AFS_VOLID_FMT "; clone aborted\n",
		afs_printable_VolumeId_lu(newId), afs_printable_VolumeId_lu(V_parentId(originalvp)));
	    newvp = (Volume *) 0;
	    goto fail;
	}
    }
    if (newType == readonlyVolume)
	V_cloneId(originalvp) = newId;
    Log("1 Volser: Clone: Cloning volume %" AFS_VOLID_FMT " to new volume %" AFS_VOLID_FMT "\n", afs_printable_VolumeId_lu(tt->volid),
	afs_printable_VolumeId_lu(newId));
    if (purgevp)
	Log("1 Volser: Clone: Purging old read only volume %" AFS_VOLID_FMT "\n", afs_printable_VolumeId_lu(purgeId));
    CloneVolume(&error, originalvp, newvp, purgevp);
    purgevp = NULL;		/* clone releases it, maybe even if error */
    if (error) {
	Log("1 Volser: Clone: clone operation failed with code %u\n", error);
	LogError(error);
	goto fail;
    }
    if (newType == readonlyVolume) {
	V_type(newvp) = readonlyVolume;
    } else if (newType == backupVolume) {
	V_type(newvp) = backupVolume;
	V_backupId(originalvp) = newId;
    }
    strcpy(V_name(newvp), newName);
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
    TClearRxCall(tt);
#ifdef AFS_DEMAND_ATTACH_FS
    salv_vp = NULL;
#endif

    /* Clients could have callbacks to the clone ID */
    FSYNC_VolOp(newId, NULL, FSYNC_VOL_BREAKCBKS, 0l, NULL);

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
        TClearRxCall(tt);
	TRELE(tt);
    }
    if (ttc)
	DeleteTrans(ttc, 1);
#ifdef AFS_DEMAND_ATTACH_FS
    if (salv_vp && error != VVOLEXISTS && error != EXDEV) {
	V_needsSalvaged(salv_vp) = 1;
    }
#endif /* AFS_DEMAND_ATTACH_FS */
    return error;
}

/* reclone this volume into the specified id */
afs_int32
SAFSVolReClone(struct rx_call *acid, afs_int32 atrans, VolumeId cloneId)
{
    afs_int32 code;

    code = VolReClone(acid, atrans, cloneId);
    osi_auditU(acid, VS_ReCloneEvent, code, AUD_LONG, atrans, AUD_LONG,
	       cloneId, AUD_END);
    return code;
}

static afs_int32
VolReClone(struct rx_call *acid, afs_int32 atrans, VolumeId cloneId)
{
    struct Volume *originalvp, *clonevp;
    Error error, code;
    afs_int32 newType;
    struct volser_trans *tt, *ttc;
    char caller[MAXKTCNAMELEN];
    VolumeDiskData saved_header;

    /*not a super user */
    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;
    if (DoLogging) {
	char buffer[16];
	Log("%s on %s is executing Reclone Volume %" AFS_VOLID_FMT "\n", caller,
	    callerAddress(acid, buffer), afs_printable_VolumeId_lu(cloneId));
    }
    error = 0;
    clonevp = originalvp = (Volume *) 0;

    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolReClone: volume %" AFS_VOLID_FMT " has been deleted \n", afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    ttc = NewTrans(cloneId, tt->partition);
    if (!ttc) {			/* someone is messing with the clone already */
	TRELE(tt);
	return VOLSERVOLBUSY;
    }
    TSetRxCall(tt, acid, "ReClone");

    originalvp = tt->volume;
    if ((V_destroyMe(originalvp) == DESTROY_ME) || !V_inService(originalvp)) {
	Log("1 Volser: Clone: Volume %" AFS_VOLID_FMT " is offline and cannot be cloned\n",
	    afs_printable_VolumeId_lu(V_id(originalvp)));
	error = VOFFLINE;
	goto fail;
    }

    clonevp = VAttachVolume_retry(&error, cloneId, V_VOLUPD);
    if (error) {
	Log("1 Volser: can't attach clone %" AFS_VOLID_FMT "\n", afs_printable_VolumeId_lu(cloneId));
	goto fail;
    }

    newType = V_type(clonevp);	/* type of the new volume */

    if (originalvp->device != clonevp->device) {
	Log("1 Volser: Clone: Volumes %" AFS_VOLID_FMT " and %" AFS_VOLID_FMT " are on different devices\n",
	    afs_printable_VolumeId_lu(tt->volid), afs_printable_VolumeId_lu(cloneId));
	error = EXDEV;
	goto fail;
    }
    if (V_parentId(originalvp) != V_parentId(clonevp)) {
	Log("1 Volser: Clone: Volume %" AFS_VOLID_FMT " was not originally cloned from volume %" AFS_VOLID_FMT "; aborted\n", afs_printable_VolumeId_lu(cloneId), afs_printable_VolumeId_lu(tt->volid));
	error = EXDEV;
	goto fail;
    }

    if (DoPreserveVolumeStats) {
	CopyVolumeStats(&V_disk(clonevp), &saved_header);
    }

    error = 0;
    Log("1 Volser: Clone: Recloning volume %" AFS_VOLID_FMT " to volume %" AFS_VOLID_FMT "\n", afs_printable_VolumeId_lu(tt->volid),
	afs_printable_VolumeId_lu(cloneId));
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

    /* update the creationDate, since this represents the last cloning date
     * for ROs. But do not update copyDate; let it stay so we can identify
     * when the clone was first created. */
    V_creationDate(clonevp) = time(0);
    if (DoPreserveVolumeStats) {
	CopyVolumeStats(&saved_header, &V_disk(clonevp));
    } else {
	ClearVolumeStats(&V_disk(clonevp));
    }
    V_destroyMe(clonevp) = 0;
    V_inService(clonevp) = 0;
    if (newType == backupVolume) {
	V_backupDate(originalvp) = V_creationDate(clonevp);
	V_backupDate(clonevp) = V_creationDate(clonevp);
    }
    V_inUse(clonevp) = 0;
    VUpdateVolume(&error, clonevp);
    if (error) {
	Log("1 Volser: Clone: VUpdate failed code %u\n", error);
	LogError(error);
	goto fail;
    }
    /* VUpdateVolume succeeded. Mark it in service so there's no window
     * between FSYNC_VOL_ON and VolSetFlags where it's offline with no
     * specialStatus; this is a reclone and this volume started online
     */
    V_inService(clonevp) = 1;
    VDetachVolume(&error, clonevp);	/* allow file server to get it's hands on it */
    clonevp = NULL;
    VUpdateVolume(&error, originalvp);
    if (error) {
	Log("1 Volser: Clone: original update %u\n", error);
	LogError(error);
	goto fail;
    }
    TClearRxCall(tt);
    if (TRELE(tt)) {
	tt = (struct volser_trans *)0;
	error = VOLSERTRELE_ERROR;
	goto fail;
    }

    DeleteTrans(ttc, 1);

    {
	struct DiskPartition64 *tpartp = originalvp->partition;
	FSYNC_VolOp(cloneId, tpartp->name, FSYNC_VOL_BREAKCBKS, 0, NULL);
    }
    return 0;

  fail:
    if (clonevp)
	VDetachVolume(&code, clonevp);
    if (tt) {
        TClearRxCall(tt);
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
SAFSVolTransCreate(struct rx_call *acid, VolumeId volume, afs_int32 partition,
		   afs_int32 iflags, afs_int32 *ttid)
{
    afs_int32 code;

    code = VolTransCreate(acid, volume, partition, iflags, ttid);
    osi_auditU(acid, VS_TransCrEvent, code, AUD_LONG, *ttid, AUD_LONG, volume,
	       AUD_END);
    return code;
}

static afs_int32
VolTransCreate(struct rx_call *acid, VolumeId volume, afs_int32 partition,
		   afs_int32 iflags, afs_int32 *ttid)
{
    struct volser_trans *tt;
    Volume *tv;
    afs_int32 error;
    Error code;
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
    VTRANS_OBJ_LOCK(tt);
    tt->volume = tv;
    *ttid = tt->tid;
    tt->iflags = iflags;
    tt->vflags = 0;
    TSetRxCall_r(tt, NULL, "TransCreate");
    VTRANS_OBJ_UNLOCK(tt);
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;
}

/* using aindex as a 0-based index, return the aindex'th volume on this server
 * Both the volume number and partition number (one-based) are returned.
 */
afs_int32
SAFSVolGetNthVolume(struct rx_call *acid, afs_int32 aindex, VolumeId *avolume,
		    afs_int32 *apart)
{
    afs_int32 code;

    code = VolGetNthVolume(acid, aindex, avolume, apart);
    osi_auditU(acid, VS_GetNVolEvent, code, AUD_LONG, *avolume, AUD_END);
    return code;
}

static afs_int32
VolGetNthVolume(struct rx_call *acid, afs_int32 aindex, afs_uint32 *avolume,
		    afs_int32 *apart)
{
    if (!afsconf_CheckRestrictedQuery(tdir, acid, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

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

static afs_int32
VolGetFlags(struct rx_call *acid, afs_int32 atid, afs_int32 *aflags)
{
    struct volser_trans *tt;

    if (!afsconf_CheckRestrictedQuery(tdir, acid, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

    tt = FindTrans(atid);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolGetFlags: volume %" AFS_VOLID_FMT " has been deleted \n",
	    afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    TSetRxCall(tt, acid, "GetFlags");
    *aflags = tt->vflags;
    TClearRxCall(tt);
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

static afs_int32
VolSetFlags(struct rx_call *acid, afs_int32 atid, afs_int32 aflags)
{
    struct volser_trans *tt;
    struct Volume *vp;
    Error error;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    /* find the trans */
    tt = FindTrans(atid);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolSetFlags: volume %" AFS_VOLID_FMT " has been deleted \n",
	    afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    TSetRxCall(tt, acid, "SetFlags");
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
    VTRANS_OBJ_LOCK(tt);
    tt->vflags = aflags;
    TClearRxCall_r(tt);
    VTRANS_OBJ_UNLOCK(tt);
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
	       htonl(destination->destHost), AUD_LONG, destTrans, AUD_END);
    return code;
}

static_inline afs_int32
MakeClient(struct rx_call *acid, struct rx_securityClass **securityObject,
	   afs_int32 *securityIndex)
{
    rxkad_level enc_level = rxkad_clear;
    int docrypt;
    int code;

    switch (doCrypt) {
    case VS2SC_ALWAYS:
	docrypt = 1;
	break;
    case VS2SC_INHERIT:
	rxkad_GetServerInfo(rx_ConnectionOf(acid), &enc_level, 0, 0, 0, 0, 0);
	docrypt = (enc_level == rxkad_crypt ? 1 : 0);
	break;
    case VS2SC_NEVER:
	docrypt = 0;
	break;
    default:
	opr_Assert(0 && "doCrypt corrupt?");
    }
    if (docrypt)
	code = afsconf_ClientAuthSecure(tdir, securityObject, securityIndex);
    else
	code = afsconf_ClientAuth(tdir, securityObject, securityIndex);
    return code;
}

static afs_int32
VolForward(struct rx_call *acid, afs_int32 fromTrans, afs_int32 fromDate,
	       struct destServer *destination, afs_int32 destTrans,
	       struct restoreCookie *cookie)
{
    struct volser_trans *tt;
    afs_int32 code;
    struct rx_connection *tcon;
    struct rx_call *tcall;
    struct Volume *vp;
    struct rx_securityClass *securityObject;
    afs_int32 securityIndex;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */

    /* find the local transaction */
    tt = FindTrans(fromTrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolForward: volume %" AFS_VOLID_FMT " has been deleted \n", afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    vp = tt->volume;
    TSetRxCall(tt, NULL, "Forward");

    /* get auth info for the this connection (uses afs from ticket file) */
    code = MakeClient(acid, &securityObject, &securityIndex);
    if (code) {
	TRELE(tt);
	return code;
    }

    /* make an rpc connection to the other server */
    tcon =
	rx_NewConnection(htonl(destination->destHost),
			 htons(destination->destPort), VOLSERVICE_ID,
			 securityObject, securityIndex);

    RXS_Close(securityObject); /* will be freed after connection destroyed */

    if (!tcon) {
        TClearRxCall(tt);
	TRELE(tt);
	return ENOTCONN;
    }
    tcall = rx_NewCall(tcon);
    TSetRxCall(tt, tcall, "Forward");
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
    TClearRxCall(tt);
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
        TClearRxCall(tt);
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

    if (results) {
	memset(results, 0, sizeof(manyResults));
	i = results->manyResults_len = destinations->manyDests_len;
	results->manyResults_val = codes = malloc(i * sizeof(afs_int32));
    }
    if (!results || !results->manyResults_val)
	return ENOMEM;

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(fromTrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolForward: volume %" AFS_VOLID_FMT " has been deleted \n", afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    vp = tt->volume;
    TSetRxCall(tt, NULL, "ForwardMulti");

    /* (fromDate == 0) ==> full dump */
    is_incremental = (fromDate ? 1 : 0);

    tcons = malloc(i * sizeof(struct rx_connection *));
    if (!tcons) {
	return ENOMEM;
    }
    tcalls = malloc(i * sizeof(struct rx_call *));
    if (!tcalls) {
	free(tcons);
	return ENOMEM;
    }

    /* get auth info for this connection (uses afs from ticket file) */
    code = MakeClient(acid, &securityObject, &securityIndex);
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

    /* Security object will be freed when all connections destroyed */
    RXS_Close(securityObject);

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
		   fromTrans, AUD_HOST, htonl(dest->server.destHost), AUD_LONG,
		   dest->trans, AUD_END);
    }
    free(tcons);
    free(tcalls);

    if (tt) {
        TClearRxCall(tt);
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
SAFSVolDumpV2(struct rx_call *acid, afs_int32 fromTrans, afs_int32 fromDate,
	      afs_int32 flags)
{
    afs_int32 code;

    code = VolDump(acid, fromTrans, fromDate, flags);
    osi_auditU(acid, VS_DumpEvent, code, AUD_LONG, fromTrans, AUD_END);
    return code;
}

static afs_int32
VolDump(struct rx_call *acid, afs_int32 fromTrans, afs_int32 fromDate,
	afs_int32 flags)
{
    int code = 0;
    struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(fromTrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolDump: volume %" AFS_VOLID_FMT " has been deleted \n", afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    TSetRxCall(tt, acid, "Dump");
    code = DumpVolume(acid, tt->volume, fromDate, (flags & VOLDUMPV2_OMITDIRS)
		      ? 0 : 1);	/* squirt out the volume's data, too */
    if (code) {
        TClearRxCall(tt);
	TRELE(tt);
	return code;
    }
    TClearRxCall(tt);

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

    code = VolRestore(acid, atrans, cookie);
    osi_auditU(acid, VS_RestoreEvent, code, AUD_LONG, atrans, AUD_END);
    return code;
}

static afs_int32
VolRestore(struct rx_call *acid, afs_int32 atrans, struct restoreCookie *cookie)
{
    struct volser_trans *tt;
    afs_int32 code, tcode;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolRestore: volume %" AFS_VOLID_FMT " has been deleted \n", afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    if (DoLogging) {
	char buffer[16];
	Log("%s on %s is executing Restore %" AFS_VOLID_FMT "\n", caller,
	    callerAddress(acid, buffer), afs_printable_VolumeId_lu(tt->volid));
    }
    TSetRxCall(tt, acid, "Restore");

    DFlushVolume(V_parentId(tt->volume)); /* Ensure dir buffers get dropped */

    code = RestoreVolume(acid, tt->volume, cookie);
    FSYNC_VolOp(tt->volid, NULL, FSYNC_VOL_BREAKCBKS, 0l, NULL);
    TClearRxCall(tt);
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

static afs_int32
VolEndTrans(struct rx_call *acid, afs_int32 destTrans, afs_int32 *rcode)
{
    struct volser_trans *tt;
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
	       htonl(anewsite), AUD_END);
    return code;
}

static afs_int32
VolSetForwarding(struct rx_call *acid, afs_int32 atid, afs_int32 anewsite)
{
    struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];
    char partName[16];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(atid);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolSetForwarding: volume %" AFS_VOLID_FMT " has been deleted \n",
	    afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    TSetRxCall(tt, acid, "SetForwarding");
    if (volutil_PartitionName2_r(tt->partition, partName, sizeof(partName)) != 0) {
	partName[0] = '\0';
    }
    FSYNC_VolOp(tt->volid, partName, FSYNC_VOL_MOVE, anewsite, NULL);
    TClearRxCall(tt);
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;
}

afs_int32
SAFSVolGetStatus(struct rx_call *acid, afs_int32 atrans,
		 struct volser_status *astatus)
{
    afs_int32 code;

    code = VolGetStatus(acid, atrans, astatus);
    osi_auditU(acid, VS_GetStatEvent, code, AUD_LONG, atrans, AUD_END);
    return code;
}

static afs_int32
VolGetStatus(struct rx_call *acid, afs_int32 atrans,
	     struct volser_status *astatus)
{
    struct Volume *tv;
    struct VolumeDiskData *td;
    struct volser_trans *tt;

    if (!afsconf_CheckRestrictedQuery(tdir, acid, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolGetStatus: volume %" AFS_VOLID_FMT " has been deleted \n",
	    afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    TSetRxCall(tt, acid, "GetStatus");
    tv = tt->volume;
    if (!tv) {
        TClearRxCall(tt);
	TRELE(tt);
	return ENOENT;
    }

    td = &(V_disk(tv));
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
    TClearRxCall(tt);
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;
}

afs_int32
SAFSVolSetInfo(struct rx_call *acid, afs_int32 atrans,
	       struct volintInfo *astatus)
{
    afs_int32 code;

    code = VolSetInfo(acid, atrans, astatus);
    osi_auditU(acid, VS_SetInfoEvent, code, AUD_LONG, atrans, AUD_END);
    return code;
}

static afs_int32
VolSetInfo(struct rx_call *acid, afs_int32 atrans,
	       struct volintInfo *astatus)
{
    struct Volume *tv;
    struct VolumeDiskData *td;
    struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];
    Error error;

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolSetInfo: volume %" AFS_VOLID_FMT " has been deleted \n", afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    TSetRxCall(tt, acid, "SetStatus");
    tv = tt->volume;
    if (!tv) {
        TClearRxCall(tt);
	TRELE(tt);
	return ENOENT;
    }

    td = &(V_disk(tv));
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
    TClearRxCall(tt);
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

static afs_int32
VolGetName(struct rx_call *acid, afs_int32 atrans, char **aname)
{
    struct Volume *tv;
    struct VolumeDiskData *td;
    struct volser_trans *tt;
    int len;

    if (!afsconf_CheckRestrictedQuery(tdir, acid, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

    /* We need to at least fill it in */
    *aname = malloc(1);
    if (!*aname)
	return ENOMEM;
    tt = FindTrans(atrans);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolGetName: volume %" AFS_VOLID_FMT " has been deleted \n", afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    TSetRxCall(tt, acid, "GetName");
    tv = tt->volume;
    if (!tv) {
        TClearRxCall(tt);
	TRELE(tt);
	return ENOENT;
    }

    td = &(V_disk(tv));
    len = strlen(td->name) + 1;	/* don't forget the null */
    if (len >= SIZE) {
        TClearRxCall(tt);
	TRELE(tt);
	return E2BIG;
    }
    *aname = realloc(*aname, len);
    strcpy(*aname, td->name);
    TClearRxCall(tt);
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

    return 0;
}

/*this is a handshake to indicate that the next call will be SAFSVolRestore
 * - a noop now !*/
afs_int32
SAFSVolSignalRestore(struct rx_call *acid, char volname[], int volType,
		     VolumeId parentId, VolumeId cloneId)
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

static afs_int32
VolListPartitions(struct rx_call *acid, struct pIDs *partIds)
{
    char namehead[9];
    int i;

    if (!afsconf_CheckRestrictedQuery(tdir, acid, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

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

static afs_int32
XVolListPartitions(struct rx_call *acid, struct partEntries *pEntries)
{
    char namehead[9];
    struct partList partList;
    struct DiskPartition64 *dp;
    int i, j = 0;

    if (!afsconf_CheckRestrictedQuery(tdir, acid, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

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
	    int k;

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
    if (j > 0) {
	pEntries->partEntries_val = malloc(j * sizeof(int));
	if (!pEntries->partEntries_val)
	    return ENOMEM;
	memcpy(pEntries->partEntries_val, partList.partId,
		j * sizeof(int));
	pEntries->partEntries_len = j;
    } else {
	pEntries->partEntries_val = NULL;
	pEntries->partEntries_len = 0;
    }
    return 0;

}

/*
 * Scan a directory for possible volume headers.
 * in: DIR *dirp		-- a directory handle from opendir()
 * out: char *volname		-- set to name of directory entry
 *      afs_uint32 *volid	-- set to volume ID parsed from name
 * returns:
 *  true if volname and volid have been set to valid values
 *  false if we got to the end of the directory
 */
static int
GetNextVol(DIR *dirp, char *volname, VolumeId *volid)
{
    struct dirent *dp;

    while ((dp = readdir(dirp)) != NULL) {
	/* could be optimized on platforms with dp->d_namlen */
	if (dp->d_name[0] == 'V' && strlen(dp->d_name) == VHDRNAMELEN
		&& strcmp(&(dp->d_name[VFORMATDIGITS + 1]), VHDREXT) == 0) {
	    *volid = VolumeNumber(dp->d_name);
	    strcpy(volname, dp->d_name);
	    return 1;
	}
    }
    return 0;
}

/**
 * volint vol info structure type.
 */
typedef enum {
    VOLINT_INFO_TYPE_BASE,  /**< volintInfo type */
    VOLINT_INFO_TYPE_EXT    /**< volintXInfo type */
} volint_info_type_t;

/**
 * handle to various on-wire vol info types.
 */
typedef struct {
    volint_info_type_t volinfo_type;
    union {
	void * opaque;
	volintInfo * base;
	volintXInfo * ext;
    } volinfo_ptr;
} volint_info_handle_t;

/**
 * store value to a field at the appropriate location in on-wire structure.
 */
#define VOLINT_INFO_STORE(handle, name, val) \
    do { \
        if ((handle)->volinfo_type == VOLINT_INFO_TYPE_BASE) { \
            (handle)->volinfo_ptr.base->name = (val); \
        } else { \
            (handle)->volinfo_ptr.ext->name = (val); \
        } \
    } while(0)

/**
 * get pointer to appropriate offset of field in on-wire structure.
 */
#define VOLINT_INFO_PTR(handle, name) \
    (((handle)->volinfo_type == VOLINT_INFO_TYPE_BASE) ? \
     &((handle)->volinfo_ptr.base->name) : \
     &((handle)->volinfo_ptr.ext->name))

/**
 * fill in appropriate type of on-wire volume metadata structure.
 *
 * @param vp      pointer to volume object
 * @param handle  pointer to wire format handle object
 *
 * @pre vp object must contain header & pending_vol_op structurs (populate if from RPC)
 * @pre handle object must have a valid pointer and enumeration value
 *
 * @note passing a NULL value for vp means that the fileserver doesn't
 *       know about this particular volume, thus implying it is offline.
 *
 * @return operation status
 *   @retval 0 success
 *   @retval 1 failure
 */
static int
FillVolInfo(Volume * vp, volint_info_handle_t * handle)
{
    unsigned int numStatBytes, now;
    struct VolumeDiskData *hdr = &(V_disk(vp));

    /*read in the relevant info */
    strcpy((char *)VOLINT_INFO_PTR(handle, name), hdr->name);
    VOLINT_INFO_STORE(handle, status, VOK);	/*its ok */
    VOLINT_INFO_STORE(handle, volid, hdr->id);
    VOLINT_INFO_STORE(handle, type, hdr->type);	/*if ro volume */
    VOLINT_INFO_STORE(handle, cloneID, hdr->cloneId);	/*if rw volume */
    VOLINT_INFO_STORE(handle, backupID, hdr->backupId);
    VOLINT_INFO_STORE(handle, parentID, hdr->parentId);
    VOLINT_INFO_STORE(handle, copyDate, hdr->copyDate);
    VOLINT_INFO_STORE(handle, size, hdr->diskused);
    VOLINT_INFO_STORE(handle, maxquota, hdr->maxquota);
    VOLINT_INFO_STORE(handle, filecount, hdr->filecount);
    now = FT_ApproxTime();
    if ((now - hdr->dayUseDate) > OneDay) {
	VOLINT_INFO_STORE(handle, dayUse, 0);
    } else {
	VOLINT_INFO_STORE(handle, dayUse, hdr->dayUse);
    }
    VOLINT_INFO_STORE(handle, creationDate, hdr->creationDate);
    VOLINT_INFO_STORE(handle, accessDate, hdr->accessDate);
    VOLINT_INFO_STORE(handle, updateDate, hdr->updateDate);
    VOLINT_INFO_STORE(handle, backupDate, hdr->backupDate);

#ifdef AFS_DEMAND_ATTACH_FS
    /*
     * for DAFS, we "lie" about volume state --
     * instead of returning the raw state from the disk header,
     * we compute state based upon the fileserver's internal
     * in-core state enumeration value reported to us via fssync,
     * along with the blessed and inService flags from the header.
     *   -- tkeiser 11/27/2007
     */

    /* Conditions that offline status is based on:
		volume is unattached state
		volume state is in (one of several error states)
		volume not in service
		volume is not marked as blessed (not on hold)
		volume in salvage req. state
		volume needsSalvaged
		next op would set volume offline
		next op would not leave volume online (based on several conditions)
    */
    if (!vp ||
	(V_attachState(vp) == VOL_STATE_UNATTACHED) ||
	VIsErrorState(V_attachState(vp)) ||
	!hdr->inService ||
	!hdr->blessed ||
	(V_attachState(vp) == VOL_STATE_SALVSYNC_REQ) ||
	hdr->needsSalvaged ||
	(vp->pending_vol_op &&
		(vp->pending_vol_op->com.command == FSYNC_VOL_OFF ||
		!VVolOpLeaveOnline_r(vp, vp->pending_vol_op) )
	)
	) {
	VOLINT_INFO_STORE(handle, inUse, 0);
    } else {
	VOLINT_INFO_STORE(handle, inUse, 1);
    }
#else
    /* offline status based on program type, where != fileServer enum (1) is offline */
    if (hdr->inUse == fileServer) {
	VOLINT_INFO_STORE(handle, inUse, 1);
    } else {
	VOLINT_INFO_STORE(handle, inUse, 0);
    }
#endif


    switch(handle->volinfo_type) {
	/* NOTE: VOLINT_INFO_STORE not used in this section because values are specific to one volinfo_type */
    case VOLINT_INFO_TYPE_BASE:

#ifdef AFS_DEMAND_ATTACH_FS
	/* see comment above where we set inUse bit */
	if (hdr->needsSalvaged ||
	    (vp && VIsErrorState(V_attachState(vp)))) {
	    handle->volinfo_ptr.base->needsSalvaged = 1;
	} else {
	    handle->volinfo_ptr.base->needsSalvaged = 0;
	}
#else
	handle->volinfo_ptr.base->needsSalvaged = hdr->needsSalvaged;
#endif
	handle->volinfo_ptr.base->destroyMe = hdr->destroyMe;
	handle->volinfo_ptr.base->spare0 = hdr->minquota;
	handle->volinfo_ptr.base->spare1 =
	    (long)hdr->weekUse[0] +
	    (long)hdr->weekUse[1] +
	    (long)hdr->weekUse[2] +
	    (long)hdr->weekUse[3] +
	    (long)hdr->weekUse[4] +
	    (long)hdr->weekUse[5] +
	    (long)hdr->weekUse[6];
	handle->volinfo_ptr.base->flags = 0;
	handle->volinfo_ptr.base->spare2 = hdr->volUpdateCounter;
	handle->volinfo_ptr.base->spare3 = 0;
	break;


    case VOLINT_INFO_TYPE_EXT:
	numStatBytes =
	    4 * ((2 * VOLINT_STATS_NUM_RWINFO_FIELDS) +
		 (4 * VOLINT_STATS_NUM_TIME_FIELDS));

	/*
	 * Copy out the stat fields in a single operation.
	 */
	if ((now - hdr->dayUseDate) > OneDay) {
	    memset(&(handle->volinfo_ptr.ext->stat_reads[0]),
		   0, numStatBytes);
	} else {
	    memcpy((char *)&(handle->volinfo_ptr.ext->stat_reads[0]),
		   (char *)&(hdr->stat_reads[0]),
		   numStatBytes);
	}
	break;
    }

    return 0;
}

#ifdef AFS_DEMAND_ATTACH_FS

/**
 * get struct Volume out of the fileserver.
 *
 * @param[in] volumeId  volumeId for which we want state information
 * @param[in] pname     partition name string
 * @param[inout] vp     pointer to pointer to Volume object which
 *                      will be populated (see note)
 *
 * @return operation status
 *   @retval 0         success
 *   @retval non-zero  failure
 *
 * @note if FSYNC_VolOp fails in certain ways, *vp will be set to NULL
 *
 * @internal
 */
static int
GetVolObject(VolumeId volumeId, char * pname, Volume ** vp)
{
    int code;
    SYNC_response res;

    res.hdr.response_len = sizeof(res.hdr);
    res.payload.buf = *vp;
    res.payload.len = sizeof(Volume);

    code = FSYNC_VolOp(volumeId,
		       pname,
		       FSYNC_VOL_QUERY,
		       0,
		       &res);

    if (code != SYNC_OK) {
	switch (res.hdr.reason) {
	case FSYNC_WRONG_PART:
	case FSYNC_UNKNOWN_VOLID:
	    *vp = NULL;
	    code = SYNC_OK;
	    break;
	}
    }

    return code;
}

#endif

/**
 * mode of volume list operation.
 */
typedef enum {
    VOL_INFO_LIST_SINGLE,   /**< performing a single volume list op */
    VOL_INFO_LIST_MULTIPLE  /**< performing a multi-volume list op */
} vol_info_list_mode_t;

/**
 * abstract interface to populate wire-format volume metadata structures.
 *
 * @param[in]  partId    partition id
 * @param[in]  volumeId  volume id
 * @param[in]  pname     partition name
 * @param[in]  volname   volume file name
 * @param[in]  handle    handle to on-wire volume metadata object
 * @param[in]  mode      listing mode
 *
 * @return operation status
 *   @retval 0      success
 *   @retval -2     DESTROY_ME flag is set
 *   @retval -1     general failure; some data filled in
 *   @retval -3     couldn't create vtrans; some data filled in
 */
static int
GetVolInfo(afs_uint32 partId,
	   VolumeId volumeId,
	   char * pname,
	   char * volname,
	   volint_info_handle_t * handle,
	   vol_info_list_mode_t mode)
{
    int code = -1;
    Error error;
    struct volser_trans *ttc = NULL;
    struct Volume *fill_tv, *tv = NULL;
#ifdef AFS_DEMAND_ATTACH_FS
    struct Volume fs_tv_buf, *fs_tv = &fs_tv_buf; /* Create a structure, and a pointer to that structure */
    SYNC_PROTO_BUF_DECL(fs_res_buf); /* Buffer for the pending_vol_op */
    SYNC_response fs_res; /* Response handle for the pending_vol_op */
    FSSYNC_VolOp_info pending_vol_op_res; /* Pending vol ops to full in volume */

    /* Set up response handle for pending_vol_op */
    fs_res.hdr.response_len = sizeof(fs_res.hdr);
    fs_res.payload.buf = fs_res_buf;
    fs_res.payload.len = SYNC_PROTO_MAX_LEN;
#endif

    ttc = NewTrans(volumeId, partId);
    if (!ttc) {
	code = -3;
	VOLINT_INFO_STORE(handle, status, VBUSY);
	VOLINT_INFO_STORE(handle, volid, volumeId);
	goto drop;
    }

    /* Get volume from volserver */
    if (mode == VOL_INFO_LIST_MULTIPLE)
	tv = VAttachVolumeByName(&error, pname, volname, V_PEEK);
    else {
#ifdef AFS_DEMAND_ATTACH_FS
	int mode = V_PEEK;
#else
	int mode = V_READONLY;   /* informs the fileserver to update the volume headers. */
#endif
	tv = VAttachVolumeByName_retry(&error, pname, volname, mode);
    }

    if (error) {
	Log("1 Volser: GetVolInfo: Could not attach volume %" AFS_VOLID_FMT " (%s:%s) error=%d\n",
	    afs_printable_VolumeId_lu(volumeId), pname, volname, error);
	goto drop;
    }

    /*
     * please note that destroyMe and needsSalvaged checks used to be ordered
     * in the opposite manner for ListVolumes and XListVolumes.  I think it's
     * more correct to check destroyMe before needsSalvaged.
     *   -- tkeiser 11/28/2007
     */

    if (V_destroyMe(tv) == DESTROY_ME) {
	switch (mode) {
	case VOL_INFO_LIST_MULTIPLE:
	    code = -2;
	    goto drop;

	case VOL_INFO_LIST_SINGLE:
	    Log("1 Volser: GetVolInfo: Volume %" AFS_VOLID_FMT " (%s:%s) will be destroyed on next salvage\n",
		afs_printable_VolumeId_lu(volumeId), pname, volname);
	    goto drop;

	default:
	    goto drop;
	}
    }

    if (V_needsSalvaged(tv)) {
	/*this volume will be salvaged */
	Log("1 Volser: GetVolInfo: Volume %" AFS_VOLID_FMT " (%s:%s) needs to be salvaged\n",
	    afs_printable_VolumeId_lu(volumeId), pname, volname);
    }

#ifdef AFS_DEMAND_ATTACH_FS
    /* If using DAFS, get volume from fsserver */
    if (GetVolObject(volumeId, pname, &fs_tv) != SYNC_OK || fs_tv == NULL) {

	goto drop;
    }

    /* fs_tv is a shallow copy, must populate certain structures before passing along */
    if (FSYNC_VolOp(volumeId, pname, FSYNC_VOL_QUERY_VOP, 0, &fs_res) == SYNC_OK) {
	/* If we if the pending vol op */
	memcpy(&pending_vol_op_res, fs_res.payload.buf, sizeof(FSSYNC_VolOp_info));
	fs_tv->pending_vol_op=&pending_vol_op_res;
    } else {
	fs_tv->pending_vol_op=NULL;
    }

    /* populate the header from the volserver copy */
    fs_tv->header=tv->header;

    /* When using DAFS, use the fs volume info, populated with required structures */
    fill_tv = fs_tv;
#else
    /* When not using DAFS, just use the local volume info */
    fill_tv = tv;
#endif

    /* ok, we have all the data we need; fill in the on-wire struct */
    code = FillVolInfo(fill_tv, handle);

 drop:
    if (code == -1) {
	VOLINT_INFO_STORE(handle, status, 0);
	strcpy((char *)VOLINT_INFO_PTR(handle, name), volname);
	VOLINT_INFO_STORE(handle, volid, volumeId);
    }
    if (tv) {
	VDetachVolume(&error, tv);
	tv = NULL;
	if (error) {
	    VOLINT_INFO_STORE(handle, status, 0);
	    strcpy((char *)VOLINT_INFO_PTR(handle, name), volname);
	    Log("1 Volser: GetVolInfo: Could not detach volume %" AFS_VOLID_FMT " (%s:%s)\n",
		afs_printable_VolumeId_lu(volumeId), pname, volname);
	}
    }
    if (ttc) {
	DeleteTrans(ttc, 1);
	ttc = NULL;
    }
    return code;
}


/*return the header information about the <volid> */
afs_int32
SAFSVolListOneVolume(struct rx_call *acid, afs_int32 partid,
                     VolumeId volumeId, volEntries *volumeInfo)
{
    afs_int32 code;

    code = VolListOneVolume(acid, partid, volumeId, volumeInfo);
    osi_auditU(acid, VS_Lst1VolEvent, code, AUD_LONG, volumeId, AUD_END);
    return code;
}

static afs_int32
VolListOneVolume(struct rx_call *acid, afs_int32 partid,
                 VolumeId volumeId, volEntries *volumeInfo)
{
    struct DiskPartition64 *partP;
    char pname[9], volname[20];
    DIR *dirp;
    VolumeId volid;
    int found = 0;
    volint_info_handle_t handle;

    if (!afsconf_CheckRestrictedQuery(tdir, acid, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

    volumeInfo->volEntries_val = calloc(1, sizeof(volintInfo));
    if (!volumeInfo->volEntries_val)
	return ENOMEM;

    volumeInfo->volEntries_len = 1;
    if (GetPartName(partid, pname))
	return VOLSERILLEGAL_PARTITION;
    if (!(partP = VGetPartition(pname, 0)))
	return VOLSERILLEGAL_PARTITION;
    dirp = opendir(VPartitionPath(partP));
    if (dirp == NULL)
	return VOLSERILLEGAL_PARTITION;

    while (GetNextVol(dirp, volname, &volid)) {
	if (volid == volumeId) {	/*copy other things too */
	    found = 1;
	    break;
	}
    }

    if (found) {
#ifndef AFS_PTHREAD_ENV
	IOMGR_Poll();	/*make sure that the client does not time out */
#endif

	handle.volinfo_type = VOLINT_INFO_TYPE_BASE;
	handle.volinfo_ptr.base = volumeInfo->volEntries_val;

	/* The return code from GetVolInfo is ignored; there is no error from
	 * it that results in the whole call being aborted. Any volume
	 * attachment failures are reported in 'status' field in the
	 * volumeInfo payload. */
	GetVolInfo(partid,
	           volid,
	           pname,
	           volname,
	           &handle,
	           VOL_INFO_LIST_SINGLE);
    }

    closedir(dirp);
    return (found) ? 0 : ENODEV;
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
		      VolumeId a_volID, volXEntries *a_volumeXInfoP)
{
    afs_int32 code;

    code = VolXListOneVolume(a_rxCidP, a_partID, a_volID, a_volumeXInfoP);
    osi_auditU(a_rxCidP, VS_XLst1VlEvent, code, AUD_LONG, a_volID, AUD_END);
    return code;
}

static afs_int32
VolXListOneVolume(struct rx_call *a_rxCidP, afs_int32 a_partID,
                  VolumeId a_volID, volXEntries *a_volumeXInfoP)
{				/*SAFSVolXListOneVolume */

    struct DiskPartition64 *partP;	/*Ptr to partition */
    char pname[9], volname[20];	/*Partition, volume names */
    DIR *dirp;			/*Partition directory ptr */
    VolumeId currVolID;	        /*Current volume ID */
    int found = 0;		/*Did we find the volume we need? */
    volint_info_handle_t handle;

    if (!afsconf_CheckRestrictedQuery(tdir, a_rxCidP, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

    /*
     * Set up our pointers for action, marking our structure to hold exactly
     * one entry.  Also, assume we'll fail in our quest.
     */
    a_volumeXInfoP->volXEntries_val = calloc(1, sizeof(volintXInfo));
    if (!a_volumeXInfoP->volXEntries_val)
	return ENOMEM;

    a_volumeXInfoP->volXEntries_len = 1;

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
    while (GetNextVol(dirp, volname, &currVolID)) {
	if (currVolID == a_volID) {
	    /*
	     * We found the volume entry we're interested.  Pull out the
	     * extended information, remembering to poll (so that the client
	     * doesn't time out) and to set up a transaction on the volume.
	     */
	    found = 1;
	    break;
	}			/*Found desired volume */
    }

    if (found) {
#ifndef AFS_PTHREAD_ENV
	IOMGR_Poll();
#endif

	handle.volinfo_type = VOLINT_INFO_TYPE_EXT;
	handle.volinfo_ptr.ext = a_volumeXInfoP->volXEntries_val;

	/* The return code from GetVolInfo is ignored; there is no error from
	 * it that results in the whole call being aborted. Any volume
	 * attachment failures are reported in 'status' field in the
	 * volumeInfo payload. */
	GetVolInfo(a_partID,
	           a_volID,
	           pname,
	           volname,
	           &handle,
	           VOL_INFO_LIST_SINGLE);
    }

    /*
     * Clean up before going to dinner: close the partition directory,
     * return the proper value.
     */
    closedir(dirp);
    return (found) ? 0 : ENODEV;
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

static afs_int32
VolListVolumes(struct rx_call *acid, afs_int32 partid, afs_int32 flags,
		   volEntries *volumeInfo)
{
    volintInfo *pntr;
    struct DiskPartition64 *partP;
    afs_int32 allocSize = 1000;	/*to be changed to a larger figure */
    char pname[9], volname[20];
    DIR *dirp;
    VolumeId volid;
    int code;
    volint_info_handle_t handle;

    if (!afsconf_CheckRestrictedQuery(tdir, acid, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

    volumeInfo->volEntries_val = calloc(allocSize, sizeof(volintInfo));
    if (!volumeInfo->volEntries_val)
	return ENOMEM;

    pntr = volumeInfo->volEntries_val;
    volumeInfo->volEntries_len = 0;
    if (GetPartName(partid, pname))
	return VOLSERILLEGAL_PARTITION;
    if (!(partP = VGetPartition(pname, 0)))
	return VOLSERILLEGAL_PARTITION;
    dirp = opendir(VPartitionPath(partP));
    if (dirp == NULL)
	return VOLSERILLEGAL_PARTITION;

    while (GetNextVol(dirp, volname, &volid)) {
	if (flags) {		/*copy other things too */
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();	/*make sure that the client does not time out */
#endif

	    handle.volinfo_type = VOLINT_INFO_TYPE_BASE;
	    handle.volinfo_ptr.base = pntr;


	    code = GetVolInfo(partid,
			      volid,
			      pname,
			      volname,
			      &handle,
			      VOL_INFO_LIST_MULTIPLE);
	    if (code == -2)	/* DESTROY_ME flag set */
		continue;
	} else {
	    pntr->volid = volid;
	    /*just volids are needed */
	}

	pntr++;
	volumeInfo->volEntries_len += 1;
	if ((allocSize - volumeInfo->volEntries_len) < 5) {
	    /*running out of space, allocate more space */
	    allocSize = (allocSize * 3) / 2;
	    pntr = realloc(volumeInfo->volEntries_val,
			   allocSize * sizeof(volintInfo));
	    if (pntr == NULL) {
		closedir(dirp);
		return VOLSERNO_MEMORY;
	    }
	    volumeInfo->volEntries_val = pntr;	/* point to new block */
	    /* set pntr to the right position */
	    pntr = volumeInfo->volEntries_val + volumeInfo->volEntries_len;
	}
    }

    closedir(dirp);
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

static afs_int32
VolXListVolumes(struct rx_call *a_rxCidP, afs_int32 a_partID,
		    afs_int32 a_flags, volXEntries *a_volumeXInfoP)
{				/*SAFSVolXListVolumes */

    volintXInfo *xInfoP;	/*Ptr to the extended vol info */
    struct DiskPartition64 *partP;	/*Ptr to partition */
    afs_int32 allocSize = 1000;	/*To be changed to a larger figure */
    char pname[9], volname[20];	/*Partition, volume names */
    DIR *dirp;			/*Partition directory ptr */
    VolumeId volid;		/*Current volume ID */
    int code;
    volint_info_handle_t handle;

    if (!afsconf_CheckRestrictedQuery(tdir, a_rxCidP, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

    /*
     * Allocate a large array of extended volume info structures, then
     * set it up for action.
     */
    a_volumeXInfoP->volXEntries_val = calloc(allocSize, sizeof(volintXInfo));
    if (!a_volumeXInfoP->volXEntries_val)
	return ENOMEM;

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
    while (GetNextVol(dirp, volname, &volid)) {
	if (a_flags) {
	    /*
	     * Full info about the volume desired.  Poll to make sure the
	     * client doesn't time out, then start up a new transaction.
	     */
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();
#endif

	    handle.volinfo_type = VOLINT_INFO_TYPE_EXT;
	    handle.volinfo_ptr.ext = xInfoP;

	    code = GetVolInfo(a_partID,
			      volid,
			      pname,
			      volname,
			      &handle,
			      VOL_INFO_LIST_MULTIPLE);
	    if (code == -2)	/* DESTROY_ME flag set */
		continue;
	} else {
	    /*
	     * Just volume IDs are needed.
	     */
	    xInfoP->volid = volid;
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
    }

    /*
     * We've examined all entries in the partition directory.  Close it,
     * delete our transaction (if any), and go home happy.
     */
    closedir(dirp);
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

static afs_int32
VolMonitor(struct rx_call *acid, transDebugEntries *transInfo)
{
    transDebugInfo *pntr;
    afs_int32 allocSize = 50;
    afs_int32 code = 0;
    struct volser_trans *tt, *nt, *allTrans;

    if (!afsconf_CheckRestrictedQuery(tdir, acid, restrictedQueryLevel))
        return VOLSERBAD_ACCESS;

    transInfo->transDebugEntries_val =
	malloc(allocSize * sizeof(transDebugInfo));
    if (!transInfo->transDebugEntries_val)
	return ENOMEM;
    pntr = transInfo->transDebugEntries_val;
    transInfo->transDebugEntries_len = 0;

    VTRANS_LOCK;
    allTrans = TransList();
    if (allTrans == (struct volser_trans *)0)
	goto done;		/*no active transactions */
    for (tt = allTrans; tt; tt = nt) {	/*copy relevant info into pntr */
	nt = tt->next;
	memset(pntr, 0, sizeof(*pntr));
        VTRANS_OBJ_LOCK(tt);
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
	    rx_GetCallStatus(tt->rxCallPtr, &(pntr->readNext), &(pntr->transmitNext),
				&(pntr->lastSendTime), &(pntr->lastReceiveTime));
	}
        VTRANS_OBJ_UNLOCK(tt);
	pntr++;
	transInfo->transDebugEntries_len += 1;
	if ((allocSize - transInfo->transDebugEntries_len) < 5) {	/*alloc some more space */
	    allocSize = (allocSize * 3) / 2;
	    pntr = realloc(transInfo->transDebugEntries_val,
			   allocSize * sizeof(transDebugInfo));
	    if (pntr == NULL) {
		code = ENOMEM;
		goto done;
	    }
	    transInfo->transDebugEntries_val = pntr;
	    pntr =
		transInfo->transDebugEntries_val +
		transInfo->transDebugEntries_len;
	    /*set pntr to right position */
	}

    }
done:
    VTRANS_UNLOCK;

    return code;
}

afs_int32
SAFSVolSetIdsTypes(struct rx_call *acid, afs_int32 atid, char name[],
		   afs_int32 type, afs_uint32 pId, VolumeId cloneId,
		   VolumeId backupId)
{
    afs_int32 code;

    code = VolSetIdsTypes(acid, atid, name, type, pId, cloneId, backupId);
    osi_auditU(acid, VS_SetIdTyEvent, code, AUD_LONG, atid, AUD_STR, name,
	       AUD_LONG, type, AUD_LONG, pId, AUD_LONG, cloneId, AUD_LONG,
	       backupId, AUD_END);
    return code;
}

static afs_int32
VolSetIdsTypes(struct rx_call *acid, afs_int32 atid, char name[],
	       afs_int32 type, VolumeId pId, VolumeId cloneId,
	       VolumeId backupId)
{
    struct Volume *tv;
    Error error = 0;
    struct volser_trans *tt;
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
	Log("1 Volser: VolSetIds: volume %" AFS_VOLID_FMT " has been deleted \n", afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    TSetRxCall(tt, acid, "SetIdsTypes");
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
    TClearRxCall(tt);
    if (TRELE(tt) && !error)
	return VOLSERTRELE_ERROR;

    return error;
  fail:
    TClearRxCall(tt);
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

static afs_int32
VolSetDate(struct rx_call *acid, afs_int32 atid, afs_int32 cdate)
{
    struct Volume *tv;
    Error error = 0;
    struct volser_trans *tt;
    char caller[MAXKTCNAMELEN];

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    /* find the trans */
    tt = FindTrans(atid);
    if (!tt)
	return ENOENT;
    if (tt->vflags & VTDeleted) {
	Log("1 Volser: VolSetDate: volume %" AFS_VOLID_FMT " has been deleted \n", afs_printable_VolumeId_lu(tt->volid));
	TRELE(tt);
	return ENOENT;
    }
    TSetRxCall(tt, acid, "SetDate");
    tv = tt->volume;

    V_creationDate(tv) = cdate;
    VUpdateVolume(&error, tv);
    if (error) {
	Log("1 Volser: SetDate: VUpdate failed code %d\n", error);
	LogError(error);
	goto fail;
    }
    TClearRxCall(tt);
    if (TRELE(tt) && !error)
	return VOLSERTRELE_ERROR;

    return error;
  fail:
    TClearRxCall(tt);
    if (TRELE(tt) && !error)
	return VOLSERTRELE_ERROR;
    return error;
}

afs_int32
SAFSVolConvertROtoRWvolume(struct rx_call *acid, afs_int32 partId,
			   VolumeId volumeId)
{
#ifdef AFS_NT40_ENV
    return EXDEV;
#else
    char caller[MAXKTCNAMELEN];
    DIR *dirp;
    struct volser_trans *ttc;
    char pname[16], volname[20];
    struct DiskPartition64 *partP;
    afs_int32 ret = ENODEV;
    VolumeId volid;

    if (!afsconf_SuperUser(tdir, acid, caller))
	return VOLSERBAD_ACCESS;	/*not a super user */
    if (GetPartName(partId, pname))
        return VOLSERILLEGAL_PARTITION;
    if (!(partP = VGetPartition(pname, 0)))
        return VOLSERILLEGAL_PARTITION;
    dirp = opendir(VPartitionPath(partP));
    if (dirp == NULL)
	return VOLSERILLEGAL_PARTITION;
    ttc = (struct volser_trans *)0;

    while (GetNextVol(dirp, volname, &volid)) {
	if (volid == volumeId) {        /*copy other things too */
#ifndef AFS_PTHREAD_ENV
            IOMGR_Poll();       /*make sure that the client doesnot time out */
#endif
            ttc = NewTrans(volumeId, partId);
            if (!ttc) {
		ret = VOLSERVOLBUSY;
		goto done;
            }
	    ret = FSYNC_VolOp(volumeId, pname, FSYNC_VOL_NEEDVOLUME, V_VOLUPD,
			      NULL);
	    if (ret != SYNC_OK) {
		Log("SAFSVolConvertROtoRWvolume: Error %ld trying to check out "
		    "vol %lu part %s.\n", afs_printable_int32_ld(ret),
		    afs_printable_uint32_lu(volumeId), pname);
		ret = VOLSERFAILEDOP;
		goto done;
	    }
#ifdef AFS_NAMEI_ENV
	    ret = namei_ConvertROtoRWvolume(pname, volumeId);
#else
	    ret = inode_ConvertROtoRWvolume(pname, volumeId);
#endif
	    if (ret != 0) {
		Log("SAFSVolConvertROtoRWvolume: Error %ld trying to convert "
		    "RO vol %lu to RW. The volume may be in an inconsistent or "
		    "partially converted state; if the volume seems unusable, "
		    "you can try salvaging it or restoring from another RO "
		    "copy.\n", afs_printable_int32_ld(ret),
		    afs_printable_uint32_lu(volumeId));
		/*
		 * the volume may or may not be usable at this point; let the
		 * fileserver try to attach and decide.
		 */
		FSYNC_VolOp(volumeId, pname, FSYNC_VOL_ON, 0, NULL);
	    }
	    break;
	}
    }
  done:
    if (ttc)
        DeleteTrans(ttc, 1);

    closedir(dirp);
    return ret;
#endif
}

afs_int32
SAFSVolGetSize(struct rx_call *acid, afs_int32 fromTrans, afs_int32 fromDate,
	       struct volintSize *size)
{
    int code = 0;
    struct volser_trans *tt;
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
    TSetRxCall(tt, acid, "GetSize");
    code = SizeDumpVolume(acid, tt->volume, fromDate, 1, size);	/* measure volume's data */
    TClearRxCall(tt);
    if (TRELE(tt))
	return VOLSERTRELE_ERROR;

/*    osi_auditU(acid, VS_DumpEvent, code, AUD_LONG, fromTrans, AUD_END);  */
    return code;
}

afs_int32
SAFSVolSplitVolume(struct rx_call *acall, afs_uint32 ovid, afs_uint32 onew,
		   afs_uint32 where, afs_int32 verbose)
{
#if defined(AFS_NAMEI_ENV) && !defined(AFS_NT40_ENV)
    Error code, code2;
    Volume *vol=0, *newvol=0;
    struct volser_trans *tt = 0, *tt2 = 0;
    char caller[MAXKTCNAMELEN];
    char line[128];
    VolumeId new = onew;
    VolumeId vid = ovid;

    if (!afsconf_SuperUser(tdir, acall, caller))
        return EPERM;

    vol = VAttachVolume(&code, vid, V_VOLUPD);
    if (!vol) {
        if (!code)
            code = ENOENT;
        return code;
    }
    newvol = VAttachVolume(&code, new, V_VOLUPD);
    if (!newvol) {
        VDetachVolume(&code2, vol);
        if (!code)
            code = ENOENT;
        return code;
    }
    if (V_device(vol) != V_device(newvol)
	|| V_uniquifier(newvol) != 2) {
        if (V_device(vol) != V_device(newvol)) {
            sprintf(line, "Volumes %" AFS_VOLID_FMT " and %" AFS_VOLID_FMT " are not in the same partition, aborted.\n",
		    afs_printable_VolumeId_lu(vid),
		    afs_printable_VolumeId_lu(new));
            rx_Write(acall, line, strlen(line));
        }
        if (V_uniquifier(newvol) != 2) {
            sprintf(line, "Volume %" AFS_VOLID_FMT " is not freshly created, aborted.\n",
		    afs_printable_VolumeId_lu(new));
            rx_Write(acall, line, strlen(line));
        }
        line[0] = 0;
        rx_Write(acall, line, 1);
        VDetachVolume(&code2, vol);
        VDetachVolume(&code2, newvol);
        return EINVAL;
    }
    tt = NewTrans(vid, V_device(vol));
    if (!tt) {
        sprintf(line, "Couldn't create transaction for %" AFS_VOLID_FMT ", aborted.\n",
		afs_printable_VolumeId_lu(vid));
        rx_Write(acall, line, strlen(line));
        line[0] = 0;
        rx_Write(acall, line, 1);
        VDetachVolume(&code2, vol);
        VDetachVolume(&code2, newvol);
        return VOLSERVOLBUSY;
    }
    VTRANS_OBJ_LOCK(tt);
    tt->iflags = ITBusy;
    tt->vflags = 0;
    TSetRxCall_r(tt, NULL, "SplitVolume");
    VTRANS_OBJ_UNLOCK(tt);

    tt2 = NewTrans(new, V_device(newvol));
    if (!tt2) {
        sprintf(line, "Couldn't create transaction for %" AFS_VOLID_FMT ", aborted.\n",
		afs_printable_VolumeId_lu(new));
        rx_Write(acall, line, strlen(line));
        line[0] = 0;
        rx_Write(acall, line, 1);
        DeleteTrans(tt, 1);
        VDetachVolume(&code2, vol);
        VDetachVolume(&code2, newvol);
        return VOLSERVOLBUSY;
    }
    VTRANS_OBJ_LOCK(tt2);
    tt2->iflags = ITBusy;
    tt2->vflags = 0;
    TSetRxCall_r(tt2, NULL, "SplitVolume");
    VTRANS_OBJ_UNLOCK(tt2);

    code = split_volume(acall, vol, newvol, where, verbose);

    VDetachVolume(&code2, vol);
    DeleteTrans(tt, 1);
    VDetachVolume(&code2, newvol);
    DeleteTrans(tt2, 1);
    return code;
#else
    return VOLSERBADOP;
#endif
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
