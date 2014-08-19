/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 * afs_vtoi (local)
 * afs_UFSGetVolSlot
 * afs_MemGetVolSlot
 * afs_CheckVolumeNames
 * afs_FindVolume
 */
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */

#if !defined(UKERNEL)
#if !defined(AFS_LINUX20_ENV)
#include <net/if.h>
#endif
#include <netinet/in.h>

#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV)
#include <netinet/in_var.h>
#endif /* ! AFS_HPUX110_ENV */
#endif /* !defined(UKERNEL) */

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#include "afs/afs_dynroot.h"

#if	defined(AFS_SUN56_ENV)
#include <inet/led.h>
#include <inet/common.h>
#if     defined(AFS_SUN58_ENV)
#include <netinet/ip6.h>
#endif
#include <inet/ip.h>
#endif

/* In case we don't have the vl error table yet. */
#ifndef ERROR_TABLE_BASE_VL
#define ERROR_TABLE_BASE_VL     (363520L)
#define VL_NOENT                (363524L)
#endif /* vlserver error base define */

/* Exported variables */
afs_dcache_id_t volumeInode;	/* Inode for VolumeItems file */
afs_rwlock_t afs_xvolume;	/** allocation lock for volumes */
struct volume *afs_freeVolList;
struct volume *afs_volumes[NVOLS];
afs_int32 afs_volCounter = 1;	/** for allocating volume indices */
afs_int32 fvTable[NFENTRIES];

/* Forward declarations */
static struct volume *afs_NewVolumeByName(char *aname, afs_int32 acell,
					  int agood, struct vrequest *areq,
					  afs_int32 locktype);
static struct volume *afs_NewDynrootVolume(struct VenusFid *fid);
static int inVolList(struct VenusFid *fid, afs_int32 nvols, afs_int32 * vID,
		     afs_int32 * cID);



/**
 * Convert a volume name to a number;
 * @param aname Volume name.
 * @return return 0 if can't parse as a number.
 */
static int
afs_vtoi(char *aname)
{
    afs_int32 temp;
    int tc;
    temp = 0;
    AFS_STATCNT(afs_vtoi);
    while ((tc = *aname++)) {
	if (tc > '9' || tc < '0')
	    return 0;		/* invalid name */
	temp *= 10;
	temp += tc - '0';
    }
    return temp;

}				/*afs_vtoi */


/**
 * All of the vol cache routines must be called with the afs_xvolume
 * lock held in exclusive mode, since they use static variables.
 * In addition, we don't want two people adding the same volume
 * at the same time.
 */

static struct fvolume staticFVolume;
afs_int32 afs_FVIndex = -1;

/**
 * UFS specific version of afs_GetVolSlot
 * @return
 */
struct volume *
afs_UFSGetVolSlot(void)
{
    struct volume *tv = NULL, **lv;
    struct osi_file *tfile;
    afs_int32 i = -1, code;
    afs_int32 bestTime;
    struct volume *bestVp, *oldLp = NULL, **bestLp = NULL;
    char *oldname = NULL;
    afs_int32 oldvtix = -2; /* Initialize to a value that doesn't occur */

    AFS_STATCNT(afs_UFSGetVolSlot);
    if (!afs_freeVolList) {
	/* get free slot */
	bestTime = 0x7fffffff;
	bestVp = 0;
	bestLp = 0;
	for (i = 0; i < NVOLS; i++) {
	    lv = &afs_volumes[i];
	    for (tv = *lv; tv; lv = &tv->next, tv = *lv) {
		if (tv->refCount == 0) {	/* is this one available? */
		    if (tv->accessTime < bestTime) {	/* best one available? */
			bestTime = tv->accessTime;
			bestLp = lv;
			bestVp = tv;
		    }
		}
	    }
	}
	if (!bestVp) {
	    afs_warn("afs_UFSGetVolSlot: no vol slots available\n");
	    goto error;
	}
	tv = bestVp;

	oldLp = *bestLp;
	*bestLp = tv->next;

	oldname = tv->name;
	tv->name = NULL;

	oldvtix = tv->vtix;
	/* now write out volume structure to file */
	if (tv->vtix < 0) {
	    tv->vtix = afs_volCounter++;
	    /* now put on hash chain */
	    i = FVHash(tv->cell, tv->volume);
	    staticFVolume.next = fvTable[i];
	    fvTable[i] = tv->vtix;
	} else {
	    /*
	     * Haul the guy in from disk so we don't overwrite hash table
	     * next chain
	     */
	    if (afs_FVIndex != tv->vtix) {
		tfile = osi_UFSOpen(&volumeInode);
		code =
		    afs_osi_Read(tfile, sizeof(struct fvolume) * tv->vtix,
				 &staticFVolume, sizeof(struct fvolume));
		osi_UFSClose(tfile);
		if (code != sizeof(struct fvolume)) {
		    afs_warn("afs_UFSGetVolSlot: error %d reading volumeinfo\n",
		             (int)code);
		    goto error;
		}
		afs_FVIndex = tv->vtix;
	    }
	}
	afs_FVIndex = tv->vtix;
	staticFVolume.volume = tv->volume;
	staticFVolume.cell = tv->cell;
	staticFVolume.mtpoint = tv->mtpoint;
	staticFVolume.dotdot = tv->dotdot;
	staticFVolume.rootVnode = tv->rootVnode;
	staticFVolume.rootUnique = tv->rootUnique;
	tfile = osi_UFSOpen(&volumeInode);
	code =
	    afs_osi_Write(tfile, sizeof(struct fvolume) * afs_FVIndex,
			  &staticFVolume, sizeof(struct fvolume));
	osi_UFSClose(tfile);
	if (code != sizeof(struct fvolume)) {
	    afs_warn("afs_UFSGetVolSlot: error %d writing volumeinfo\n",
	             (int)code);
	    goto error;
	}
	if (oldname) {
	    afs_osi_Free(oldname, strlen(oldname) + 1);
	    oldname = NULL;
	}
    } else {
	tv = afs_freeVolList;
	afs_freeVolList = tv->next;
    }
    return tv;

 error:
    if (tv) {
	if (oldvtix == -2) {
	    afs_warn("afs_UFSGetVolSlot: oldvtix is uninitialized\n");
	    return NULL;
	}
	if (oldname) {
	    tv->name = oldname;
	    oldname = NULL;
	}
	if (oldvtix < 0) {
	    afs_volCounter--;
	    fvTable[i] = staticFVolume.next;
	}
	if (bestLp) {
	    *bestLp = oldLp;
	}
	tv->vtix = oldvtix;
	/* we messed with staticFVolume, so make sure someone else
	 * doesn't think it's fine to use */
	afs_FVIndex = -1;
    }
    return NULL;
}				/*afs_UFSGetVolSlot */


/**
 *   Get an available volume list slot. If the list does not exist,
 * create one containing a single element.
 * @return
 */
struct volume *
afs_MemGetVolSlot(void)
{
    struct volume *tv;

    AFS_STATCNT(afs_MemGetVolSlot);
    if (!afs_freeVolList) {
	struct volume *newVp;

	newVp = afs_osi_Alloc(sizeof(struct volume));
	osi_Assert(newVp != NULL);

	newVp->next = NULL;
	afs_freeVolList = newVp;
    }
    tv = afs_freeVolList;
    afs_freeVolList = tv->next;
    return tv;

}				/*afs_MemGetVolSlot */

/*!
 * Reset volume information for all volume structs that
 * point to a speicific server, skipping a given volume if provided.
 *
 * @param[in] srvp
 *      The server to reset volume info about
 * @param[in] tv
 *      The volume to skip resetting info about
 */
void
afs_ResetVolumes(struct server *srvp, struct volume *tv)
{
    int j, k;
    struct volume *vp;

    /* Find any volumes residing on this server and flush their state */
    for (j = 0; j < NVOLS; j++) {
	for (vp = afs_volumes[j]; vp; vp = vp->next) {
	    for (k = 0; k < AFS_MAXHOSTS; k++) {
		if (!srvp || (vp->serverHost[k] == srvp)) {
		    if (tv && tv != vp) {
			vp->serverHost[k] = 0;
			afs_ResetVolumeInfo(vp);
		    }
		    break;
		}
	    }
	}
    }
}

/**
 *   Reset volume name to volume id mapping cache.
 * @param flags
 */
void
afs_CheckVolumeNames(int flags)
{
    afs_int32 i, j;
    struct volume *tv;
    unsigned int now;
    struct vcache *tvc;
    afs_int32 *volumeID, *cellID, vsize, nvols;
#ifdef AFS_DARWIN80_ENV
    vnode_t tvp;
#endif
    AFS_STATCNT(afs_CheckVolumeNames);

    nvols = 0;
    volumeID = cellID = NULL;
    vsize = 0;
    ObtainReadLock(&afs_xvolume);
    if (flags & AFS_VOLCHECK_EXPIRED) {
	/*
	 * allocate space to hold the volumeIDs and cellIDs, only if
	 * we will be invalidating the mountpoints later on
	 */
	for (i = 0; i < NVOLS; i++)
	    for (tv = afs_volumes[i]; tv; tv = tv->next)
		++vsize;

	volumeID = afs_osi_Alloc(2 * vsize * sizeof(*volumeID));
	cellID = (volumeID) ? volumeID + vsize : 0;
    }

    now = osi_Time();
    for (i = 0; i < NVOLS; i++) {
	for (tv = afs_volumes[i]; tv; tv = tv->next) {
	    if (flags & AFS_VOLCHECK_EXPIRED) {
		if (((tv->expireTime < (now + 10)) && (tv->states & VRO))
		    || (flags & AFS_VOLCHECK_FORCE)) {
		    afs_ResetVolumeInfo(tv);	/* also resets status */
		    if (volumeID) {
			volumeID[nvols] = tv->volume;
			cellID[nvols] = tv->cell;
		    }
		    ++nvols;
		    continue;
		}
	    }
	    /* ??? */
	    if (flags & (AFS_VOLCHECK_BUSY | AFS_VOLCHECK_FORCE)) {
		for (j = 0; j < AFS_MAXHOSTS; j++)
		    tv->status[j] = not_busy;
	    }

	}
    }
    ReleaseReadLock(&afs_xvolume);


    /* next ensure all mt points are re-evaluated */
    if (nvols || (flags & (AFS_VOLCHECK_FORCE | AFS_VOLCHECK_MTPTS))) {
loop:
	ObtainReadLock(&afs_xvcache);
	for (i = 0; i < VCSIZE; i++) {
	    for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {

		/* if the volume of "mvid" of the vcache entry is among the
		 * ones we found earlier, then we re-evaluate it.  Also, if the
		 * force bit is set or we explicitly asked to reevaluate the
		 * mt-pts, we clean the cmvalid bit */

		if ((flags & (AFS_VOLCHECK_FORCE | AFS_VOLCHECK_MTPTS))
		    || (tvc->mvid
			&& inVolList(tvc->mvid, nvols, volumeID, cellID)))
		    tvc->f.states &= ~CMValid;

		/* If the volume that this file belongs to was reset earlier,
		 * then we should remove its callback.
		 * Again, if forced, always do it.
		 */
		if ((tvc->f.states & CRO)
		    && (inVolList(&tvc->f.fid, nvols, volumeID, cellID)
			|| (flags & AFS_VOLCHECK_FORCE))) {

                    if (tvc->f.states & CVInit) {
                        ReleaseReadLock(&afs_xvcache);
			afs_osi_Sleep(&tvc->f.states);
                        goto loop;
                    }
#ifdef AFS_DARWIN80_ENV
                    if (tvc->f.states & CDeadVnode) {
                        ReleaseReadLock(&afs_xvcache);
			afs_osi_Sleep(&tvc->f.states);
                        goto loop;
                    }
		    tvp = AFSTOV(tvc);
		    if (vnode_get(tvp))
			continue;
		    if (vnode_ref(tvp)) {
			AFS_GUNLOCK();
			/* AFSTOV(tvc) may be NULL */
			vnode_put(tvp);
			AFS_GLOCK();
			continue;
		    }
#else
		    AFS_FAST_HOLD(tvc);
#endif
		    ReleaseReadLock(&afs_xvcache);

		    ObtainWriteLock(&afs_xcbhash, 485);
		    /* LOCKXXX: We aren't holding tvc write lock? */
		    afs_DequeueCallback(tvc);
		    tvc->f.states &= ~CStatd;
		    ReleaseWriteLock(&afs_xcbhash);
		    if (tvc->f.fid.Fid.Vnode & 1 || (vType(tvc) == VDIR))
			osi_dnlc_purgedp(tvc);

#ifdef AFS_DARWIN80_ENV
		    vnode_put(AFSTOV(tvc));
		    /* our tvc ptr is still good until now */
		    AFS_FAST_RELE(tvc);
		    ObtainReadLock(&afs_xvcache);
#else
		    ObtainReadLock(&afs_xvcache);

		    /* our tvc ptr is still good until now */
		    AFS_FAST_RELE(tvc);
#endif
		}
	    }
	}
	osi_dnlc_purge();	/* definitely overkill, but it's safer this way. */
	ReleaseReadLock(&afs_xvcache);
    }

    if (volumeID)
	afs_osi_Free(volumeID, 2 * vsize * sizeof(*volumeID));

}				/*afs_CheckVolumeNames */


/**
 * Check if volume is in the specified list.
 * @param fid File FID.
 * @param nvols Nomber of volumes???
 * @param vID Array of volume IDs.
 * @param cID Array of cache IDs.
 * @return 1 - true, 0 - false.
 */
static int
inVolList(struct VenusFid *fid, afs_int32 nvols, afs_int32 * vID,
	  afs_int32 * cID)
{
    afs_int32 i;

    /* if no arrays present, be conservative and return true */
    if (nvols && (!vID || !cID))
	return 1;

    for (i = 0; i < nvols; ++i) {
	if (fid->Fid.Volume == vID[i] && fid->Cell == cID[i])
	    return 1;
    }
    return 0;
}


/* afs_PutVolume is now a macro in afs.h */


/**
 *    Return volume struct if we have it cached and it's up-to-date.
 *  Environment: Must be called with afs_xvolume unlocked.
 *  @param afid Volume FID.
 *  @param locktype
 *  @return Volume or NULL if no result.
 */
struct volume *
afs_FindVolume(struct VenusFid *afid, afs_int32 locktype)
{
    struct volume *tv;
    afs_int32 i;

    if (afid == NULL)
	return NULL;

    i = VHash(afid->Fid.Volume);
    ObtainWriteLock(&afs_xvolume, 106);
    for (tv = afs_volumes[i]; tv; tv = tv->next) {
	if (tv->volume == afid->Fid.Volume && tv->cell == afid->Cell
	    && (tv->states & VRecheck) == 0) {
	    tv->refCount++;
	    break;
	}
    }
    ReleaseWriteLock(&afs_xvolume);
    return tv;			/* NULL if we didn't find it */
}				/*afs_FindVolume */



/**
 *   Note that areq may be null, in which case we don't bother to set any
 * request status information.
 * @param afid Volume FID.
 * @param areq Request type.
 * @param locktype Lock to be used.
 * @return Volume or NULL if no result.
 */
struct volume *
afs_GetVolume(struct VenusFid *afid, struct vrequest *areq,
	      afs_int32 locktype)
{
    struct volume *tv;
    char *bp, tbuf[CVBS];
    AFS_STATCNT(afs_GetVolume);

    tv = afs_FindVolume(afid, locktype);
    if (!tv) {
	/* Do a dynroot check and add dynroot volume if found. */
	if (afs_IsDynrootAnyFid(afid)) {
	    tv = afs_NewDynrootVolume(afid);
	} else {
	    bp = afs_cv2string(&tbuf[CVBS], afid->Fid.Volume);
	    tv = afs_NewVolumeByName(bp, afid->Cell, 0, areq, locktype);
	}
    }
    return tv;
}				/*afs_GetVolume */



/**
 *
 * @param volid Volume ID. If it's 0, get it from the name.
 * @param aname Volume name.
 * @param ve Volume entry.
 * @param tcell The cell containing this volume.
 * @param agood
 * @param type Type of volume.
 * @param areq Request.
 * @return Volume or NULL if failure.
 */
static struct volume *
afs_SetupVolume(afs_int32 volid, char *aname, void *ve, struct cell *tcell,
		afs_int32 agood, afs_int32 type, struct vrequest *areq)
{
    struct volume *tv;
    struct vldbentry *ove = (struct vldbentry *)ve;
    struct nvldbentry *nve = (struct nvldbentry *)ve;
    struct uvldbentry *uve = (struct uvldbentry *)ve;

    int whichType;		/* which type of volume to look for */
    int i, j, err = 0;

    if (!volid) {
	int len;
	/* special hint from file server to use vlserver */
	len = strlen(aname);
	if (len >= 8 && strcmp(aname + len - 7, ".backup") == 0)
	    whichType = BACKVOL;
	else if (len >= 10 && strcmp(aname + len - 9, ".readonly") == 0)
	    whichType = ROVOL;
	else
	    whichType = RWVOL;

	/* figure out which one we're really interested in (a set is returned) */
	volid = afs_vtoi(aname);
	if (volid == 0) {
	    if (type == 2) {
		volid = uve->volumeId[whichType];
	    } else if (type == 1) {
		volid = nve->volumeId[whichType];
	    } else {
		volid = ove->volumeId[whichType];
	    }
	} /* end of if (volid == 0) */
    } /* end of if (!volid) */


    ObtainWriteLock(&afs_xvolume, 108);
    i = VHash(volid);
    for (tv = afs_volumes[i]; tv; tv = tv->next) {
	if (tv->volume == volid && tv->cell == tcell->cellNum) {
	    break;
	}
    }
    if (!tv) {
	struct fvolume *tf = 0;

	tv = afs_GetVolSlot();
	if (!tv) {
	    ReleaseWriteLock(&afs_xvolume);
	    return NULL;
	}
	memset(tv, 0, sizeof(struct volume));

	for (j = fvTable[FVHash(tcell->cellNum, volid)]; j != 0; j = tf->next) {
	    if (afs_FVIndex != j) {
		struct osi_file *tfile;
	        tfile = osi_UFSOpen(&volumeInode);
		err =
		    afs_osi_Read(tfile, sizeof(struct fvolume) * j,
				 &staticFVolume, sizeof(struct fvolume));
		osi_UFSClose(tfile);
		if (err != sizeof(struct fvolume)) {
		    afs_warn("afs_SetupVolume: error %d reading volumeinfo\n",
		             (int)err);
		    /* put tv back on the free list; the data in it is not valid */
		    tv->next = afs_freeVolList;
		    afs_freeVolList = tv;
		    /* staticFVolume contents are not valid */
		    afs_FVIndex = -1;
		    ReleaseWriteLock(&afs_xvolume);
		    return NULL;
		}
		afs_FVIndex = j;
	    }
	    tf = &staticFVolume;
	    if (tf->cell == tcell->cellNum && tf->volume == volid)
		break;
	}

	tv->cell = tcell->cellNum;
	AFS_RWLOCK_INIT(&tv->lock, "volume lock");
	tv->next = afs_volumes[i];	/* thread into list */
	afs_volumes[i] = tv;
	tv->volume = volid;

	if (tf && (j != 0)) {
	    tv->vtix = afs_FVIndex;
	    tv->mtpoint = tf->mtpoint;
	    tv->dotdot = tf->dotdot;
	    tv->rootVnode = tf->rootVnode;
	    tv->rootUnique = tf->rootUnique;
	} else {
	    tv->vtix = -1;
	    tv->rootVnode = tv->rootUnique = 0;
            afs_GetDynrootMountFid(&tv->dotdot);
            afs_GetDynrootMountFid(&tv->mtpoint);
            tv->mtpoint.Fid.Vnode =
              VNUM_FROM_TYPEID(VN_TYPE_MOUNT, tcell->cellIndex << 2);
            tv->mtpoint.Fid.Unique = volid;
	}
    }
    tv->refCount++;
    tv->states &= ~VRecheck;	/* just checked it */
    tv->accessTime = osi_Time();
    ReleaseWriteLock(&afs_xvolume);
    if (type == 2) {
	LockAndInstallUVolumeEntry(tv, uve, tcell->cellNum, tcell, areq);
    } else if (type == 1)
	LockAndInstallNVolumeEntry(tv, nve, tcell->cellNum);
    else
	LockAndInstallVolumeEntry(tv, ove, tcell->cellNum);
    if (agood) {
	if (!tv->name) {
	    tv->name = afs_osi_Alloc(strlen(aname) + 1);
	    osi_Assert(tv->name != NULL);
	    strcpy(tv->name, aname);
	}
    }
    for (i = 0; i < NMAXNSERVERS; i++) {
	tv->status[i] = not_busy;
    }
    ReleaseWriteLock(&tv->lock);
    return tv;
}


/**
 * Seek volume by it's name and attributes.
 * If volume not found, try to add one.
 * @param aname Volume name.
 * @param acell Cell
 * @param agood
 * @param areq
 * @param locktype Type of lock to be used.
 * @return
 */
struct volume *
afs_GetVolumeByName(char *aname, afs_int32 acell, int agood,
		    struct vrequest *areq, afs_int32 locktype)
{
    afs_int32 i;
    struct volume *tv;

    AFS_STATCNT(afs_GetVolumeByName);
    ObtainWriteLock(&afs_xvolume, 112);
    for (i = 0; i < NVOLS; i++) {
	for (tv = afs_volumes[i]; tv; tv = tv->next) {
	    if (tv->name && !strcmp(aname, tv->name) && tv->cell == acell
		&& (tv->states & VRecheck) == 0) {
		tv->refCount++;
		ReleaseWriteLock(&afs_xvolume);
		return tv;
	    }
	}
    }

    ReleaseWriteLock(&afs_xvolume);

    if (AFS_IS_DISCONNECTED)
        return NULL;

    tv = afs_NewVolumeByName(aname, acell, agood, areq, locktype);
    return (tv);
}

/**
 *   Init a new dynroot volume.
 * @param Volume FID.
 * @return Volume or NULL if not found.
 */
static struct volume *
afs_NewDynrootVolume(struct VenusFid *fid)
{
    struct cell *tcell;
    struct volume *tv;
    struct vldbentry *tve;
    char *bp, tbuf[CVBS];

    tcell = afs_GetCell(fid->Cell, READ_LOCK);
    if (!tcell)
	return NULL;
    tve = afs_osi_Alloc(sizeof(*tve));
    osi_Assert(tve != NULL);
    if (!(tcell->states & CHasVolRef))
	tcell->states |= CHasVolRef;

    bp = afs_cv2string(&tbuf[CVBS], fid->Fid.Volume);
    memset(tve, 0, sizeof(*tve));
    strcpy(tve->name, "local-dynroot");
    tve->volumeId[ROVOL] = fid->Fid.Volume;
    tve->flags = VLF_ROEXISTS;

    tv = afs_SetupVolume(0, bp, tve, tcell, 0, 0, 0);
    afs_PutCell(tcell, READ_LOCK);
    afs_osi_Free(tve, sizeof(*tve));
    return tv;
}

int lastnvcode;

/**
 * @param aname Volume name.
 * @param acell Cell id.
 * @param agood
 * @param areq Request type.
 * @param locktype Type of lock to be used.
 * @return Volume or NULL if failure.
 */
static struct volume *
afs_NewVolumeByName(char *aname, afs_int32 acell, int agood,
		    struct vrequest *areq, afs_int32 locktype)
{
    afs_int32 code, type = 0;
    struct volume *tv, *tv1;
    struct vldbentry *tve;
    struct nvldbentry *ntve;
    struct uvldbentry *utve;
    struct cell *tcell;
    char *tbuffer, *ve;
    struct afs_conn *tconn;
    struct vrequest *treq = NULL;
    struct rx_connection *rxconn;

    if (strlen(aname) > VL_MAXNAMELEN)	/* Invalid volume name */
	return NULL;

    tcell = afs_GetCell(acell, READ_LOCK);
    if (!tcell) {
	return NULL;
    }

    code = afs_CreateReq(&treq, afs_osi_credp);	/* *must* be unauth for vldb */
    if (code) {
	return NULL;
    }

    /* allow null request if we don't care about ENODEV/ETIMEDOUT distinction */
    if (!areq)
	areq = treq;


    afs_Trace2(afs_iclSetp, CM_TRACE_GETVOL, ICL_TYPE_STRING, aname,
	       ICL_TYPE_POINTER, aname);
    tbuffer = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    tve = (struct vldbentry *)(tbuffer + 1024);
    ntve = (struct nvldbentry *)tve;
    utve = (struct uvldbentry *)tve;

    do {
	tconn =
	    afs_ConnByMHosts(tcell->cellHosts, tcell->vlport, tcell->cellNum,
			     treq, SHARED_LOCK, 0, &rxconn);
	if (tconn) {
	    if (tconn->srvr->server->flags & SNO_LHOSTS) {
		type = 0;
		RX_AFS_GUNLOCK();
		code = VL_GetEntryByNameO(rxconn, aname, tve);
		RX_AFS_GLOCK();
	    } else if (tconn->srvr->server->flags & SYES_LHOSTS) {
		type = 1;
		RX_AFS_GUNLOCK();
		code = VL_GetEntryByNameN(rxconn, aname, ntve);
		RX_AFS_GLOCK();
	    } else {
		type = 2;
		RX_AFS_GUNLOCK();
		code = VL_GetEntryByNameU(rxconn, aname, utve);
		RX_AFS_GLOCK();
		if (!(tconn->srvr->server->flags & SVLSRV_UUID)) {
		    if (code == RXGEN_OPCODE) {
			type = 1;
			RX_AFS_GUNLOCK();
			code = VL_GetEntryByNameN(rxconn, aname, ntve);
			RX_AFS_GLOCK();
			if (code == RXGEN_OPCODE) {
			    type = 0;
			    tconn->srvr->server->flags |= SNO_LHOSTS;
			    RX_AFS_GUNLOCK();
			    code = VL_GetEntryByNameO(rxconn, aname, tve);
			    RX_AFS_GLOCK();
			} else if (!code)
			    tconn->srvr->server->flags |= SYES_LHOSTS;
		    } else if (!code)
			tconn->srvr->server->flags |= SVLSRV_UUID;
		}
		lastnvcode = code;
	    }
	} else
	    code = -1;
    } while (afs_Analyze(tconn, rxconn, code, NULL, treq, -1,	/* no op code for this */
			 SHARED_LOCK, tcell));

    if (code) {
	/* If the client has yet to contact this cell and contact failed due
	 * to network errors, mark the VLDB servers as back up.
	 * That the client tried and failed can be determined from the
	 * fact that there was a downtime incident, but CHasVolRef is not set.
	 */
    /* RT 48959 - unclear if this should really go */
#if 0
	if (areq->networkError && !(tcell->states & CHasVolRef)) {
	    int i;
	    struct server *sp;
	    struct srvAddr *sap;
	    for (i = 0; i < AFS_MAXCELLHOSTS; i++) {
		if ((sp = tcell->cellHosts[i]) == NULL)
		    break;
		for (sap = sp->addr; sap; sap = sap->next_sa)
		    afs_MarkServerUpOrDown(sap, 0);
	    }
	}
#endif
	afs_CopyError(treq, areq);
	osi_FreeLargeSpace(tbuffer);
	afs_PutCell(tcell, READ_LOCK);
	afs_DestroyReq(treq);
	return NULL;
    }
    /*
     * Check to see if this cell has not yet referenced a volume.  If
     * it hasn't, it's just about to change its status, and we need to mark
     * this fact down. Note that it is remotely possible that afs_SetupVolume
     * could fail and we would still not have a volume reference.
     */
    if (!(tcell->states & CHasVolRef)) {
	tcell->states |= CHasVolRef;
	afs_stats_cmperf.numCellsContacted++;
    }
    /*First time a volume in this cell has been referenced */
    if (type == 2)
	ve = (char *)utve;
    else if (type == 1)
	ve = (char *)ntve;
    else
	ve = (char *)tve;
    tv = afs_SetupVolume(0, aname, ve, tcell, agood, type, treq);
    if ((agood == 3) && tv && tv->backVol) {
	/*
	 * This means that very soon we'll ask for the BK volume so
	 * we'll prefetch it (well we did already.)
	 */
	tv1 =
	    afs_SetupVolume(tv->backVol, (char *)0, ve, tcell, 0, type, treq);
	if (tv1) {
	    tv1->refCount--;
	}
    }
    if ((agood >= 2) && tv && tv->roVol) {
	/*
	 * This means that very soon we'll ask for the RO volume so
	 * we'll prefetch it (well we did already.)
	 */
	tv1 = afs_SetupVolume(tv->roVol, NULL, ve, tcell, 0, type, treq);
	if (tv1) {
	    tv1->refCount--;
	}
    }
    osi_FreeLargeSpace(tbuffer);
    afs_PutCell(tcell, READ_LOCK);
    afs_DestroyReq(treq);
    return tv;

}				/*afs_NewVolumeByName */



/**
 *   Call this with the volume structure locked; used for new-style vldb requests.
 * @param av Volume
 * @param ve
 * @param acell
 */
void
LockAndInstallVolumeEntry(struct volume *av, struct vldbentry *ve, int acell)
{
    struct server *ts;
    struct cell *cellp;
    int i, j;
    afs_int32 mask;
    afs_uint32 temp;
    char types = 0;
    struct server *serverHost[AFS_MAXHOSTS];

    AFS_STATCNT(InstallVolumeEntry);

    memset(serverHost, 0, sizeof(serverHost));

    /* Determine the type of volume we want */
    if ((ve->flags & VLF_RWEXISTS) && (av->volume == ve->volumeId[RWVOL])) {
	mask = VLSF_RWVOL;
    } else if ((ve->flags & VLF_ROEXISTS)
	       && (av->volume == ve->volumeId[ROVOL])) {
	mask = VLSF_ROVOL;
	types |= VRO;
    } else if ((ve->flags & VLF_BACKEXISTS)
	       && (av->volume == ve->volumeId[BACKVOL])) {
	/* backup always is on the same volume as parent */
	mask = VLSF_RWVOL;
	types |= (VRO | VBackup);
    } else {
	mask = 0;		/* Can't find volume in vldb entry */
    }

    cellp = afs_GetCell(acell, 0);

    /* Step through the VLDB entry making sure each server listed is there */
    for (i = 0, j = 0; i < ve->nServers; i++) {
	if (((ve->serverFlags[i] & mask) == 0)
	    || (ve->serverFlags[i] & VLSF_DONTUSE)) {
	    continue;		/* wrong volume or  don't use this volume */
	}

	temp = htonl(ve->serverNumber[i]);
	ts = afs_GetServer(&temp, 1, acell, cellp->fsport, WRITE_LOCK,
			   (afsUUID *) 0, 0, av);
	serverHost[j] = ts;

	/*
	 * The cell field could be 0 if the server entry was created
	 * first with the 'fs setserverprefs' call which doesn't set
	 * the cell field. Thus if the afs_GetServer call above
	 * follows later on it will find the server entry thus it will
	 * simply return without setting any fields, so we set the
	 * field ourselves below.
	 */
	if (!ts->cell)
	    ts->cell = cellp;
	afs_PutServer(ts, WRITE_LOCK);
	j++;
    }

    ObtainWriteLock(&av->lock, 109);

    memcpy(av->serverHost, serverHost, sizeof(serverHost));

    /* from above */
    av->states |= types;

    /* fill in volume types */
    av->rwVol = ((ve->flags & VLF_RWEXISTS) ? ve->volumeId[RWVOL] : 0);
    av->roVol = ((ve->flags & VLF_ROEXISTS) ? ve->volumeId[ROVOL] : 0);
    av->backVol = ((ve->flags & VLF_BACKEXISTS) ? ve->volumeId[BACKVOL] : 0);

    if (ve->flags & VLF_DFSFILESET)
	av->states |= VForeign;

    afs_SortServers(av->serverHost, AFS_MAXHOSTS);
}				/*InstallVolumeEntry */


void
LockAndInstallNVolumeEntry(struct volume *av, struct nvldbentry *ve, int acell)
{
    struct server *ts;
    struct cell *cellp;
    int i, j;
    afs_int32 mask;
    afs_uint32 temp;
    char types = 0;
    struct server *serverHost[AFS_MAXHOSTS];

    AFS_STATCNT(InstallVolumeEntry);

    memset(serverHost, 0, sizeof(serverHost));

    /* Determine type of volume we want */
    if ((ve->flags & VLF_RWEXISTS) && (av->volume == ve->volumeId[RWVOL])) {
	mask = VLSF_RWVOL;
    } else if ((ve->flags & VLF_ROEXISTS)
	       && (av->volume == ve->volumeId[ROVOL])) {
	mask = VLSF_ROVOL;
	types |= VRO;
    } else if ((ve->flags & VLF_BACKEXISTS)
	       && (av->volume == ve->volumeId[BACKVOL])) {
	/* backup always is on the same volume as parent */
	mask = VLSF_RWVOL;
	types |= (VRO | VBackup);
    } else {
	mask = 0;		/* Can't find volume in vldb entry */
    }

    cellp = afs_GetCell(acell, 0);

    /* Step through the VLDB entry making sure each server listed is there */
    for (i = 0, j = 0; i < ve->nServers; i++) {
	if (((ve->serverFlags[i] & mask) == 0)
	    || (ve->serverFlags[i] & VLSF_DONTUSE)) {
	    continue;		/* wrong volume or don't use this volume */
	}

	temp = htonl(ve->serverNumber[i]);
	ts = afs_GetServer(&temp, 1, acell, cellp->fsport, WRITE_LOCK,
			   (afsUUID *) 0, 0, av);
	serverHost[j] = ts;
	/*
	 * The cell field could be 0 if the server entry was created
	 * first with the 'fs setserverprefs' call which doesn't set
	 * the cell field. Thus if the afs_GetServer call above
	 * follows later on it will find the server entry thus it will
	 * simply return without setting any fields, so we set the
	 * field ourselves below.
	 */
	if (!ts->cell)
	    ts->cell = cellp;
	afs_PutServer(ts, WRITE_LOCK);
	j++;
    }

    ObtainWriteLock(&av->lock, 110);

    memcpy(av->serverHost, serverHost, sizeof(serverHost));

    /* from above */
    av->states |= types;

    /* fill in volume types */
    av->rwVol = ((ve->flags & VLF_RWEXISTS) ? ve->volumeId[RWVOL] : 0);
    av->roVol = ((ve->flags & VLF_ROEXISTS) ? ve->volumeId[ROVOL] : 0);
    av->backVol = ((ve->flags & VLF_BACKEXISTS) ? ve->volumeId[BACKVOL] : 0);

    if (ve->flags & VLF_DFSFILESET)
	av->states |= VForeign;

    afs_SortServers(av->serverHost, AFS_MAXHOSTS);
}				/*InstallNVolumeEntry */


void
LockAndInstallUVolumeEntry(struct volume *av, struct uvldbentry *ve, int acell,
			   struct cell *tcell, struct vrequest *areq)
{
    struct server *ts;
    struct afs_conn *tconn;
    struct cell *cellp;
    int i, j;
    afs_uint32 serverid;
    afs_int32 mask;
    int k;
    char type = 0;
    struct server *serverHost[AFS_MAXHOSTS];

    AFS_STATCNT(InstallVolumeEntry);

    memset(serverHost, 0, sizeof(serverHost));

    /* Determine type of volume we want */
    if ((ve->flags & VLF_RWEXISTS) && (av->volume == ve->volumeId[RWVOL])) {
	mask = VLSF_RWVOL;
    } else if ((ve->flags & VLF_ROEXISTS)
	       && av->volume == ve->volumeId[ROVOL]) {
	mask = VLSF_ROVOL;
	type |= VRO;
    } else if ((ve->flags & VLF_BACKEXISTS)
	       && (av->volume == ve->volumeId[BACKVOL])) {
	/* backup always is on the same volume as parent */
	mask = VLSF_RWVOL;
	type |= (VRO | VBackup);
    } else {
	mask = 0;		/* Can't find volume in vldb entry */
    }

    cellp = afs_GetCell(acell, 0);

    /* Gather the list of servers the VLDB says the volume is on
     * and initialize the ve->serverHost[] array. If a server struct
     * is not found, then get the list of addresses for the
     * server, VL_GetAddrsU(), and create a server struct, afs_GetServer().
     */
    for (i = 0, j = 0; i < ve->nServers; i++) {
	if (((ve->serverFlags[i] & mask) == 0)
	    || (ve->serverFlags[i] & VLSF_DONTUSE)) {
	    continue;		/* wrong volume don't use this volume */
	}

	if (!(ve->serverFlags[i] & VLSERVER_FLAG_UUID)) {
	    /* The server has no uuid */
	    serverid = htonl(ve->serverNumber[i].time_low);
	    ts = afs_GetServer(&serverid, 1, acell, cellp->fsport,
			       WRITE_LOCK, (afsUUID *) 0, 0, av);
	} else {
	    ts = afs_FindServer(0, cellp->fsport, &ve->serverNumber[i], 0);
	    if (ts && (ts->sr_addr_uniquifier == ve->serverUnique[i])
		&& ts->addr) {
		/* uuid, uniquifier, and portal are the same */
	    } else {
		afs_uint32 *addrp, code;
		afs_int32 nentries, unique;
		bulkaddrs addrs;
		ListAddrByAttributes attrs;
		afsUUID uuid;
		struct rx_connection *rxconn;

		memset(&attrs, 0, sizeof(attrs));
		attrs.Mask = VLADDR_UUID;
		attrs.uuid = ve->serverNumber[i];
		memset(&uuid, 0, sizeof(uuid));
		memset(&addrs, 0, sizeof(addrs));
		do {
		    tconn =
			afs_ConnByMHosts(tcell->cellHosts, tcell->vlport,
					 tcell->cellNum, areq, SHARED_LOCK,
					 0, &rxconn);
		    if (tconn) {
			RX_AFS_GUNLOCK();
			code =
			    VL_GetAddrsU(rxconn, &attrs, &uuid, &unique,
					 &nentries, &addrs);
			RX_AFS_GLOCK();
		    } else {
			code = -1;
		    }

		    /* Handle corrupt VLDB (defect 7393) */
		    if (code == 0 && nentries == 0)
			code = VL_NOENT;

		} while (afs_Analyze
			 (tconn, rxconn, code, NULL, areq, -1, SHARED_LOCK, tcell));
		if (code) {
		    /* Better handing of such failures; for now we'll simply retry this call */
		    areq->volumeError = 1;
		    return;
		}

		addrp = addrs.bulkaddrs_val;
		for (k = 0; k < nentries; k++) {
		    addrp[k] = htonl(addrp[k]);
		}
		ts = afs_GetServer(addrp, nentries, acell,
				   cellp->fsport, WRITE_LOCK,
				   &ve->serverNumber[i],
				   ve->serverUnique[i], av);
		xdr_free((xdrproc_t) xdr_bulkaddrs, &addrs);
	    }
	}
	serverHost[j] = ts;

	/* The cell field could be 0 if the server entry was created
	 * first with the 'fs setserverprefs' call which doesn't set
	 * the cell field. Thus if the afs_GetServer call above
	 * follows later on it will find the server entry thus it will
	 * simply return without setting any fields, so we set the
	 * field ourselves below.
	 */
	if (!ts->cell)
	    ts->cell = cellp;
	afs_PutServer(ts, WRITE_LOCK);
	j++;
    }

    ObtainWriteLock(&av->lock, 111);

    memcpy(av->serverHost, serverHost, sizeof(serverHost));

    /* from above */
    av->states |= type;

    /* fill in volume types */
    av->rwVol = ((ve->flags & VLF_RWEXISTS) ? ve->volumeId[RWVOL] : 0);
    av->roVol = ((ve->flags & VLF_ROEXISTS) ? ve->volumeId[ROVOL] : 0);
    av->backVol = ((ve->flags & VLF_BACKEXISTS) ? ve->volumeId[BACKVOL] : 0);

    if (ve->flags & VLF_DFSFILESET)
	av->states |= VForeign;

    afs_SortServers(av->serverHost, AFS_MAXHOSTS);
}				/*InstallVolumeEntry */


/**
 *   Reset volume info for the specified volume strecture. Mark volume
 * to be rechecked next time.
 * @param tv
 */
void
afs_ResetVolumeInfo(struct volume *tv)
{
    int i;

    AFS_STATCNT(afs_ResetVolumeInfo);
    ObtainWriteLock(&tv->lock, 117);
    tv->states |= VRecheck;

    /* the hard-mount code in afs_Analyze may not be able to reset this flag
     * when VRecheck is set, so clear it here to ensure it gets cleared. */
    tv->states &= ~VHardMount;

    for (i = 0; i < AFS_MAXHOSTS; i++)
	tv->status[i] = not_busy;
    if (tv->name) {
	afs_osi_Free(tv->name, strlen(tv->name) + 1);
	tv->name = NULL;
    }
    ReleaseWriteLock(&tv->lock);
}
