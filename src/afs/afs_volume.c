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

RCSID
    ("$Header: /cvs/openafs/src/afs/afs_volume.c,v 1.26.2.5 2006/02/18 04:09:34 shadow Exp $");

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
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN60_ENV)
#include <netinet/in_var.h>
#endif /* ! AFS_HPUX110_ENV */
#endif /* !defined(UKERNEL) */

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

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
ino_t volumeInode;		/*Inode for VolumeItems file */
afs_rwlock_t afs_xvolume;	/* allocation lock for volumes */
struct volume *afs_freeVolList;
struct volume *afs_volumes[NVOLS];
afs_int32 afs_volCounter = 1;	/* for allocating volume indices */
afs_int32 fvTable[NFENTRIES];

/* Forward declarations */
static struct volume *afs_NewVolumeByName(char *aname, afs_int32 acell,
					  int agood, struct vrequest *areq,
					  afs_int32 locktype);
static struct volume *afs_NewDynrootVolume(struct VenusFid *fid);
static int inVolList(struct VenusFid *fid, afs_int32 nvols, afs_int32 * vID,
		     afs_int32 * cID);


/* Convert a volume name to a #; return 0 if can't parse as a number */
static int
afs_vtoi(register char *aname)
{
    register afs_int32 temp;
    register int tc;
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


/*
 * All of the vol cache routines must be called with the afs_xvolume
 * lock held in exclusive mode, since they use static variables.
 * In addition, we don't want two people adding the same volume
 * at the same time.
 */

static struct fvolume staticFVolume;
afs_int32 afs_FVIndex = -1;

/* UFS specific version of afs_GetVolSlot */

struct volume *
afs_UFSGetVolSlot(void)
{
    register struct volume *tv, **lv;
    struct osi_file *tfile;
    register afs_int32 i, code;
    afs_int32 bestTime;
    struct volume *bestVp, **bestLp;

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
	if (!bestVp)
	    osi_Panic("getvolslot none");
	tv = bestVp;
	*bestLp = tv->next;
	if (tv->name)
	    afs_osi_Free(tv->name, strlen(tv->name) + 1);
	tv->name = NULL;
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
		tfile = osi_UFSOpen(volumeInode);
		code =
		    afs_osi_Read(tfile, sizeof(struct fvolume) * tv->vtix,
				 &staticFVolume, sizeof(struct fvolume));
		if (code != sizeof(struct fvolume))
		    osi_Panic("read volumeinfo");
		osi_UFSClose(tfile);
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
	tfile = osi_UFSOpen(volumeInode);
	code =
	    afs_osi_Write(tfile, sizeof(struct fvolume) * afs_FVIndex,
			  &staticFVolume, sizeof(struct fvolume));
	if (code != sizeof(struct fvolume))
	    osi_Panic("write volumeinfo");
	osi_UFSClose(tfile);
    } else {
	tv = afs_freeVolList;
	afs_freeVolList = tv->next;
    }
    return tv;

}				/*afs_UFSGetVolSlot */


struct volume *
afs_MemGetVolSlot(void)
{
    register struct volume *tv;

    AFS_STATCNT(afs_MemGetVolSlot);
    if (!afs_freeVolList) {
	struct volume *newVp;

	newVp = (struct volume *)afs_osi_Alloc(sizeof(struct volume));

	newVp->next = NULL;
	afs_freeVolList = newVp;
    }
    tv = afs_freeVolList;
    afs_freeVolList = tv->next;
    return tv;

}				/*afs_MemGetVolSlot */

/* afs_ResetVolumes()
 * Reset volume inforamation for all volume structs that
 * point to a speicific server.
 */
void
afs_ResetVolumes(struct server *srvp)
{
    int j, k;
    struct volume *vp;

    /* Find any volumes residing on this server and flush their state */
    for (j = 0; j < NVOLS; j++) {
	for (vp = afs_volumes[j]; vp; vp = vp->next) {
	    for (k = 0; k < MAXHOSTS; k++) {
		if (!srvp || (vp->serverHost[k] == srvp)) {
		    vp->serverHost[k] = 0;
		    afs_ResetVolumeInfo(vp);
		    break;
		}
	    }
	}
    }
}


/* reset volume name to volume id mapping  cache */
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

	volumeID = (afs_int32 *) afs_osi_Alloc(2 * vsize * sizeof(*volumeID));
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
	    if (flags & (AFS_VOLCHECK_BUSY | AFS_VOLCHECK_FORCE)) {
		for (j = 0; j < MAXHOSTS; j++)
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
		    tvc->states &= ~CMValid;

		/* If the volume that this file belongs to was reset earlier,
		 * then we should remove its callback.
		 * Again, if forced, always do it.
		 */
		if ((tvc->states & CRO)
		    && (inVolList(&tvc->fid, nvols, volumeID, cellID)
			|| (flags & AFS_VOLCHECK_FORCE))) {

                    if (tvc->states & CVInit) {
                        ReleaseReadLock(&afs_xvcache);
			afs_osi_Sleep(&tvc->states);
                        goto loop;
                    }
#ifdef AFS_DARWIN80_ENV
                    if (tvc->states & CDeadVnode) {
                        ReleaseReadLock(&afs_xvcache);
			afs_osi_Sleep(&tvc->states);
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
		    tvc->states &= ~CStatd;
		    ReleaseWriteLock(&afs_xcbhash);
		    if (tvc->fid.Fid.Vnode & 1 || (vType(tvc) == VDIR))
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


/* afs_FindVolume()
 *  return volume struct if we have it cached and it's up-to-date
 *
 *  Environment:  Must be called with afs_xvolume unlocked.
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



/*
 * Note that areq may be null, in which case we don't bother to set any
 * request status information.
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
	if (afs_IsDynrootFid(afid)) {
	    tv = afs_NewDynrootVolume(afid);
	} else {
	    bp = afs_cv2string(&tbuf[CVBS], afid->Fid.Volume);
	    tv = afs_NewVolumeByName(bp, afid->Cell, 0, areq, locktype);
	}
    }
    return tv;
}				/*afs_GetVolume */



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
	}
    }


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
	memset((char *)tv, 0, sizeof(struct volume));
	tv->cell = tcell->cellNum;
	RWLOCK_INIT(&tv->lock, "volume lock");
	tv->next = afs_volumes[i];	/* thread into list */
	afs_volumes[i] = tv;
	tv->volume = volid;
	for (j = fvTable[FVHash(tv->cell, volid)]; j != 0; j = tf->next) {
	    if (afs_FVIndex != j) {
		struct osi_file *tfile;
		tfile = osi_UFSOpen(volumeInode);
		err =
		    afs_osi_Read(tfile, sizeof(struct fvolume) * j,
				 &staticFVolume, sizeof(struct fvolume));
		if (err != sizeof(struct fvolume))
		    osi_Panic("read volumeinfo2");
		osi_UFSClose(tfile);
		afs_FVIndex = j;
	    }
	    tf = &staticFVolume;
	    if (tf->cell == tv->cell && tf->volume == volid)
		break;
	}
	if (tf && (j != 0)) {
	    tv->vtix = afs_FVIndex;
	    tv->mtpoint = tf->mtpoint;
	    tv->dotdot = tf->dotdot;
	    tv->rootVnode = tf->rootVnode;
	    tv->rootUnique = tf->rootUnique;
	} else {
	    tv->vtix = -1;
	    tv->rootVnode = tv->rootUnique = 0;
	}
    }
    tv->refCount++;
    tv->states &= ~VRecheck;	/* just checked it */
    tv->accessTime = osi_Time();
    ReleaseWriteLock(&afs_xvolume);
    ObtainWriteLock(&tv->lock, 111);
    if (type == 2) {
	InstallUVolumeEntry(tv, uve, tcell->cellNum, tcell, areq);
    } else if (type == 1)
	InstallNVolumeEntry(tv, nve, tcell->cellNum);
    else
	InstallVolumeEntry(tv, ove, tcell->cellNum);
    if (agood) {
	if (!tv->name) {
	    tv->name = afs_osi_Alloc(strlen(aname) + 1);
	    strcpy(tv->name, aname);
	}
    }
    for (i = 0; i < NMAXNSERVERS; i++) {
	tv->status[i] = not_busy;
    }
    ReleaseWriteLock(&tv->lock);
    return tv;
}


struct volume *
afs_GetVolumeByName(register char *aname, afs_int32 acell, int agood,
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

    tv = afs_NewVolumeByName(aname, acell, agood, areq, locktype);
    return (tv);
}

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
    struct conn *tconn;
    struct vrequest treq;

    if (strlen(aname) > VL_MAXNAMELEN)	/* Invalid volume name */
	return NULL;

    tcell = afs_GetCell(acell, READ_LOCK);
    if (!tcell) {
	return NULL;
    }

    /* allow null request if we don't care about ENODEV/ETIMEDOUT distinction */
    if (!areq)
	areq = &treq;


    afs_Trace2(afs_iclSetp, CM_TRACE_GETVOL, ICL_TYPE_STRING, aname,
	       ICL_TYPE_POINTER, aname);
    tbuffer = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    tve = (struct vldbentry *)(tbuffer + 1024);
    ntve = (struct nvldbentry *)tve;
    utve = (struct uvldbentry *)tve;
    afs_InitReq(&treq, afs_osi_credp);	/* *must* be unauth for vldb */
    do {
	tconn =
	    afs_ConnByMHosts(tcell->cellHosts, tcell->vlport, tcell->cellNum,
			     &treq, SHARED_LOCK);
	if (tconn) {
	    if (tconn->srvr->server->flags & SNO_LHOSTS) {
		type = 0;
		RX_AFS_GUNLOCK();
		code = VL_GetEntryByNameO(tconn->id, aname, tve);
		RX_AFS_GLOCK();
	    } else if (tconn->srvr->server->flags & SYES_LHOSTS) {
		type = 1;
		RX_AFS_GUNLOCK();
		code = VL_GetEntryByNameN(tconn->id, aname, ntve);
		RX_AFS_GLOCK();
	    } else {
		type = 2;
		RX_AFS_GUNLOCK();
		code = VL_GetEntryByNameU(tconn->id, aname, utve);
		RX_AFS_GLOCK();
		if (!(tconn->srvr->server->flags & SVLSRV_UUID)) {
		    if (code == RXGEN_OPCODE) {
			type = 1;
			RX_AFS_GUNLOCK();
			code = VL_GetEntryByNameN(tconn->id, aname, ntve);
			RX_AFS_GLOCK();
			if (code == RXGEN_OPCODE) {
			    type = 0;
			    tconn->srvr->server->flags |= SNO_LHOSTS;
			    RX_AFS_GUNLOCK();
			    code = VL_GetEntryByNameO(tconn->id, aname, tve);
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
    } while (afs_Analyze(tconn, code, NULL, &treq, -1,	/* no op code for this */
			 SHARED_LOCK, tcell));

    if (code) {
	/* If the client has yet to contact this cell and contact failed due
	 * to network errors, mark the VLDB servers as back up.
	 * That the client tried and failed can be determined from the
	 * fact that there was a downtime incident, but CHasVolRef is not set.
	 */
	if (areq->networkError && !(tcell->states & CHasVolRef)) {
	    int i;
	    struct server *sp;
	    struct srvAddr *sap;
	    for (i = 0; i < MAXCELLHOSTS; i++) {
		if ((sp = tcell->cellHosts[i]) == NULL)
		    break;
		for (sap = sp->addr; sap; sap = sap->next_sa)
		    afs_MarkServerUpOrDown(sap, 0);
	    }
	}
	afs_CopyError(&treq, areq);
	osi_FreeLargeSpace(tbuffer);
	afs_PutCell(tcell, READ_LOCK);
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
    tv = afs_SetupVolume(0, aname, ve, tcell, agood, type, areq);
    if ((agood == 3) && tv->backVol) {
	/*
	 * This means that very soon we'll ask for the BK volume so
	 * we'll prefetch it (well we did already.)
	 */
	tv1 =
	    afs_SetupVolume(tv->backVol, (char *)0, ve, tcell, 0, type, areq);
	tv1->refCount--;
    }
    if ((agood >= 2) && tv->roVol) {
	/*
	 * This means that very soon we'll ask for the RO volume so
	 * we'll prefetch it (well we did already.)
	 */
	tv1 = afs_SetupVolume(tv->roVol, NULL, ve, tcell, 0, type, areq);
	tv1->refCount--;
    }
    osi_FreeLargeSpace(tbuffer);
    afs_PutCell(tcell, READ_LOCK);
    return tv;

}				/*afs_NewVolumeByName */



/* call this with the volume structure locked; used for new-style vldb requests */
void
InstallVolumeEntry(struct volume *av, struct vldbentry *ve, int acell)
{
    register struct server *ts;
    struct cell *cellp;
    register int i, j;
    afs_int32 mask;
    afs_uint32 temp;

    AFS_STATCNT(InstallVolumeEntry);

    /* Determine the type of volume we want */
    if ((ve->flags & VLF_RWEXISTS) && (av->volume == ve->volumeId[RWVOL])) {
	mask = VLSF_RWVOL;
    } else if ((ve->flags & VLF_ROEXISTS)
	       && (av->volume == ve->volumeId[ROVOL])) {
	mask = VLSF_ROVOL;
	av->states |= VRO;
    } else if ((ve->flags & VLF_BACKEXISTS)
	       && (av->volume == ve->volumeId[BACKVOL])) {
	/* backup always is on the same volume as parent */
	mask = VLSF_RWVOL;
	av->states |= (VRO | VBackup);
    } else {
	mask = 0;		/* Can't find volume in vldb entry */
    }

    /* fill in volume types */
    av->rwVol = ((ve->flags & VLF_RWEXISTS) ? ve->volumeId[RWVOL] : 0);
    av->roVol = ((ve->flags & VLF_ROEXISTS) ? ve->volumeId[ROVOL] : 0);
    av->backVol = ((ve->flags & VLF_BACKEXISTS) ? ve->volumeId[BACKVOL] : 0);

    if (ve->flags & VLF_DFSFILESET)
	av->states |= VForeign;

    cellp = afs_GetCell(acell, 0);

    /* This volume, av, is locked. Zero out the serverHosts[] array 
     * so that if afs_GetServer() decides to replace the server 
     * struct, we don't deadlock trying to afs_ResetVolumeInfo()
     * this volume.
     */
    for (j = 0; j < MAXHOSTS; j++) {
	av->serverHost[j] = 0;
    }

    /* Step through the VLDB entry making sure each server listed is there */
    for (i = 0, j = 0; i < ve->nServers; i++) {
	if (((ve->serverFlags[i] & mask) == 0)
	    || (ve->serverFlags[i] & VLSF_DONTUSE)) {
	    continue;		/* wrong volume or  don't use this volume */
	}

	temp = htonl(ve->serverNumber[i]);
	ts = afs_GetServer(&temp, 1, acell, cellp->fsport, WRITE_LOCK,
			   (afsUUID *) 0, 0);
	av->serverHost[j] = ts;

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
    if (j < MAXHOSTS) {
	av->serverHost[j++] = 0;
    }
    afs_SortServers(av->serverHost, MAXHOSTS);
}				/*InstallVolumeEntry */


void
InstallNVolumeEntry(struct volume *av, struct nvldbentry *ve, int acell)
{
    register struct server *ts;
    struct cell *cellp;
    register int i, j;
    afs_int32 mask;
    afs_uint32 temp;

    AFS_STATCNT(InstallVolumeEntry);

    /* Determine type of volume we want */
    if ((ve->flags & VLF_RWEXISTS) && (av->volume == ve->volumeId[RWVOL])) {
	mask = VLSF_RWVOL;
    } else if ((ve->flags & VLF_ROEXISTS)
	       && (av->volume == ve->volumeId[ROVOL])) {
	mask = VLSF_ROVOL;
	av->states |= VRO;
    } else if ((ve->flags & VLF_BACKEXISTS)
	       && (av->volume == ve->volumeId[BACKVOL])) {
	/* backup always is on the same volume as parent */
	mask = VLSF_RWVOL;
	av->states |= (VRO | VBackup);
    } else {
	mask = 0;		/* Can't find volume in vldb entry */
    }

    /* fill in volume types */
    av->rwVol = ((ve->flags & VLF_RWEXISTS) ? ve->volumeId[RWVOL] : 0);
    av->roVol = ((ve->flags & VLF_ROEXISTS) ? ve->volumeId[ROVOL] : 0);
    av->backVol = ((ve->flags & VLF_BACKEXISTS) ? ve->volumeId[BACKVOL] : 0);

    if (ve->flags & VLF_DFSFILESET)
	av->states |= VForeign;

    cellp = afs_GetCell(acell, 0);

    /* This volume, av, is locked. Zero out the serverHosts[] array 
     * so that if afs_GetServer() decides to replace the server 
     * struct, we don't deadlock trying to afs_ResetVolumeInfo()
     * this volume.
     */
    for (j = 0; j < MAXHOSTS; j++) {
	av->serverHost[j] = 0;
    }

    /* Step through the VLDB entry making sure each server listed is there */
    for (i = 0, j = 0; i < ve->nServers; i++) {
	if (((ve->serverFlags[i] & mask) == 0)
	    || (ve->serverFlags[i] & VLSF_DONTUSE)) {
	    continue;		/* wrong volume or don't use this volume */
	}

	temp = htonl(ve->serverNumber[i]);
	ts = afs_GetServer(&temp, 1, acell, cellp->fsport, WRITE_LOCK,
			   (afsUUID *) 0, 0);
	av->serverHost[j] = ts;
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
    if (j < MAXHOSTS) {
	av->serverHost[j++] = 0;
    }
    afs_SortServers(av->serverHost, MAXHOSTS);
}				/*InstallNVolumeEntry */


void
InstallUVolumeEntry(struct volume *av, struct uvldbentry *ve, int acell,
		    struct cell *tcell, struct vrequest *areq)
{
    register struct server *ts;
    struct conn *tconn;
    struct cell *cellp;
    register int i, j;
    afs_uint32 serverid;
    afs_int32 mask;
    int k;

    AFS_STATCNT(InstallVolumeEntry);

    /* Determine type of volume we want */
    if ((ve->flags & VLF_RWEXISTS) && (av->volume == ve->volumeId[RWVOL])) {
	mask = VLSF_RWVOL;
    } else if ((ve->flags & VLF_ROEXISTS)
	       && av->volume == ve->volumeId[ROVOL]) {
	mask = VLSF_ROVOL;
	av->states |= VRO;
    } else if ((ve->flags & VLF_BACKEXISTS)
	       && (av->volume == ve->volumeId[BACKVOL])) {
	/* backup always is on the same volume as parent */
	mask = VLSF_RWVOL;
	av->states |= (VRO | VBackup);
    } else {
	mask = 0;		/* Can't find volume in vldb entry */
    }

    /* fill in volume types */
    av->rwVol = ((ve->flags & VLF_RWEXISTS) ? ve->volumeId[RWVOL] : 0);
    av->roVol = ((ve->flags & VLF_ROEXISTS) ? ve->volumeId[ROVOL] : 0);
    av->backVol = ((ve->flags & VLF_BACKEXISTS) ? ve->volumeId[BACKVOL] : 0);

    if (ve->flags & VLF_DFSFILESET)
	av->states |= VForeign;

    cellp = afs_GetCell(acell, 0);

    /* This volume, av, is locked. Zero out the serverHosts[] array 
     * so that if afs_GetServer() decides to replace the server 
     * struct, we don't deadlock trying to afs_ResetVolumeInfo()
     * this volume.
     */
    for (j = 0; j < MAXHOSTS; j++) {
	av->serverHost[j] = 0;
    }

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
	    ts = afs_GetServer(&serverid, 1, acell, cellp->fsport, WRITE_LOCK,
			       (afsUUID *) 0, 0);
	} else {
	    ts = afs_FindServer(0, cellp->fsport, &ve->serverNumber[i], 0);
	    if (ts && (ts->sr_addr_uniquifier == ve->serverUnique[i])
		&& ts->addr) {
		/* uuid, uniquifier, and portal are the same */
	    } else {
		afs_uint32 *addrp, nentries, code, unique;
		bulkaddrs addrs;
		ListAddrByAttributes attrs;
		afsUUID uuid;

		memset((char *)&attrs, 0, sizeof(attrs));
		attrs.Mask = VLADDR_UUID;
		attrs.uuid = ve->serverNumber[i];
		memset((char *)&uuid, 0, sizeof(uuid));
		memset((char *)&addrs, 0, sizeof(addrs));
		do {
		    tconn =
			afs_ConnByMHosts(tcell->cellHosts, tcell->vlport,
					 tcell->cellNum, areq, SHARED_LOCK);
		    if (tconn) {
			RX_AFS_GUNLOCK();
			code =
			    VL_GetAddrsU(tconn->id, &attrs, &uuid, &unique,
					 &nentries, &addrs);
			RX_AFS_GLOCK();
		    } else {
			code = -1;
		    }

		    /* Handle corrupt VLDB (defect 7393) */
		    if (code == 0 && nentries == 0)
			code = VL_NOENT;

		} while (afs_Analyze
			 (tconn, code, NULL, areq, -1, SHARED_LOCK, tcell));
		if (code) {
		    /* Better handing of such failures; for now we'll simply retry this call */
		    areq->volumeError = 1;
		    return;
		}

		addrp = addrs.bulkaddrs_val;
		for (k = 0; k < nentries; k++) {
		    addrp[k] = htonl(addrp[k]);
		}
		ts = afs_GetServer(addrp, nentries, acell, cellp->fsport,
				   WRITE_LOCK, &ve->serverNumber[i],
				   ve->serverUnique[i]);
		afs_osi_Free(addrs.bulkaddrs_val,
			     addrs.bulkaddrs_len * sizeof(*addrp));
	    }
	}
	av->serverHost[j] = ts;

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

    afs_SortServers(av->serverHost, MAXHOSTS);
}				/*InstallVolumeEntry */


void
afs_ResetVolumeInfo(struct volume *tv)
{
    int i;

    AFS_STATCNT(afs_ResetVolumeInfo);
    ObtainWriteLock(&tv->lock, 117);
    tv->states |= VRecheck;
    for (i = 0; i < MAXHOSTS; i++)
	tv->status[i] = not_busy;
    if (tv->name) {
	afs_osi_Free(tv->name, strlen(tv->name) + 1);
	tv->name = NULL;
    }
    ReleaseWriteLock(&tv->lock);
}
