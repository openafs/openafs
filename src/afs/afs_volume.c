/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*
 * afs_volume.c
 *
 * Implements:
 * afs_vtoi (local)
 * afs_UFSGetVolSlot
 * afs_MemGetVolSlot
 * afs_CheckVolumeNames
 * afs_FindVolume
 */
#include "../afs/param.h"	/* Should be always first */
#include "../afs/stds.h"
#include "../afs/sysincludes.h"	/* Standard vendor system headers */

#if !defined(UKERNEL)
#include <net/if.h>
#include <netinet/in.h>

#ifdef AFS_SGI62_ENV
#include "../h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV)
#include <netinet/in_var.h>
#endif /* ! AFS_HPUX110_ENV */
#endif /* !defined(UKERNEL) */

#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* afs statistics */

#if	defined(AFS_SUN56_ENV)
#include <inet/led.h>
#include <inet/common.h>
#include <inet/ip.h>
#endif

/* Imported variables */
extern afs_rwlock_t afs_xserver;
extern afs_rwlock_t afs_xsrvAddr; 
extern afs_rwlock_t afs_xvcache;
extern afs_rwlock_t afs_xcbhash;
extern struct srvAddr *afs_srvAddrs[NSERVERS];  /* Hashed by server's ip */
extern int afs_totalSrvAddrs;

/* In case we don't have the vl error table yet. */
#ifndef ERROR_TABLE_BASE_vl
#define ERROR_TABLE_BASE_vl	(363520L)
#define VL_NOENT		(363524L)
#endif /* vlserver error base define */


/* Exported variables */
ino_t volumeInode;			/*Inode for VolumeItems file*/
afs_rwlock_t afs_xvolume;               /* allocation lock for volumes */
struct volume *afs_freeVolList;
struct volume *afs_volumes[NVOLS];
afs_int32 afs_volCounter = 1; /* for allocating volume indices */
afs_int32 fvTable[NFENTRIES];

/* Forward declarations */
static struct volume *afs_NewVolumeByName(char *aname, afs_int32 acell, int agood,
				   struct vrequest *areq, afs_int32 locktype);
static inVolList();


/* Convert a volume name to a #; return 0 if can't parse as a number */
static int afs_vtoi(aname)
    register char *aname;

{
    register afs_int32 temp;
    register int tc;
    temp = 0;
    AFS_STATCNT(afs_vtoi);
    while(tc = *aname++) {
	if (tc > '9' ||	tc < '0')
	    return 0; /* invalid name */
	temp *= 10;
	temp += tc - '0';
    }
    return temp;

} /*afs_vtoi*/


/*
 * All of the vol cache routines must be called with the afs_xvolume
 * lock held in exclusive mode, since they use static variables.
 * In addition, we don't want two people adding the same volume
 * at the same time.
 */

static struct fvolume staticFVolume;
afs_int32 afs_FVIndex = -1;

/* UFS specific version of afs_GetVolSlot */

struct volume *afs_UFSGetVolSlot()

{
    register struct volume *tv, **lv;
    register char *tfile;
    register afs_int32 i, code;
    afs_int32 bestTime;
    struct volume *bestVp, **bestLp;

    AFS_STATCNT(afs_UFSGetVolSlot);
    if (!afs_freeVolList) {
	/* get free slot */
	bestTime = 0x7fffffff;
	bestVp = 0;
	bestLp = 0;
	for (i=0;i<NVOLS;i++) {
	    lv = &afs_volumes[i];
	    for (tv = *lv; tv; lv = &tv->next, tv = *lv) {
		if (tv->refCount == 0) {    /* is this one available? */
		    if (tv->accessTime < bestTime) { /* best one available? */
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
	    afs_osi_Free(tv->name, strlen(tv->name)+1);
	tv->name = (char *) 0;
	/* now write out volume structure to file */
	if (tv->vtix < 0) {
	    tv->vtix = afs_volCounter++;
	    /* now put on hash chain */
	    i = FVHash(tv->cell, tv->volume);
	    staticFVolume.next = fvTable[i];
	    fvTable[i]=tv->vtix;
	}
	else {
	    /*
	     * Haul the guy in from disk so we don't overwrite hash table
	     * next chain
	     */
	    if (afs_FVIndex != tv->vtix) {
		tfile = osi_UFSOpen(volumeInode);
		code = afs_osi_Read(tfile, sizeof(struct fvolume) * tv->vtix,
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
	code = afs_osi_Write(tfile, sizeof(struct fvolume) * afs_FVIndex, 
			 &staticFVolume, sizeof(struct fvolume));
	if (code != sizeof(struct fvolume))
	    osi_Panic("write volumeinfo");
	osi_UFSClose(tfile);    
    }
    else {
	tv = afs_freeVolList;
	afs_freeVolList = tv->next;
    }
    return tv;

} /*afs_UFSGetVolSlot*/


struct volume *afs_MemGetVolSlot()

{
    register struct volume *tv, **lv;
    register afs_int32 i;
    afs_int32 bestTime;
    struct volume *bestVp, **bestLp;

    AFS_STATCNT(afs_MemGetVolSlot);
    if (!afs_freeVolList) {
	struct volume *newVp;

	newVp = (struct volume *) afs_osi_Alloc(sizeof(struct volume));

	newVp->next = (struct volume *)0;
	afs_freeVolList = newVp;
    }
    tv = afs_freeVolList;
    afs_freeVolList = tv->next;
    return tv;

} /*afs_MemGetVolSlot*/

/* afs_ResetVolumes()
 * Reset volume inforamation for all volume structs that
 * point to a speicific server.
 */
void afs_ResetVolumes(struct server *srvp)
{
  int j, k;
  struct volume *vp;

  /* Find any volumes residing on this server and flush their state */
  for (j=0; j<NVOLS; j++) {
     for (vp=afs_volumes[j]; vp; vp=vp->next) {
	for (k=0; k<MAXHOSTS; k++) {
	   if (!srvp || (vp->serverHost[k] == srvp)) {
	      afs_ResetVolumeInfo(vp);
	      break;
	   }
	}
     }
  }
}


/* reset volume name to volume id mapping  cache */
void afs_CheckVolumeNames(flags) 
    int flags;
{
    afs_int32 i,j;
    extern int osi_dnlc_purge();
    struct volume *tv;
    unsigned int now;
    struct vcache *tvc;
    afs_int32 *volumeID, *cellID, vsize, nvols;
    AFS_STATCNT(afs_CheckVolumeNames);

    nvols = 0;
    volumeID = cellID = NULL;
    vsize=0;
    ObtainReadLock(&afs_xvolume);
    if (flags & AFS_VOLCHECK_EXPIRED) {
	/*
	 * allocate space to hold the volumeIDs and cellIDs, only if
	 * we will be invalidating the mountpoints later on
	 */
	for (i=0; i< NVOLS; i++)
	    for (tv = afs_volumes[i]; tv; tv=tv->next)
		++vsize;

	volumeID = (afs_int32 *) afs_osi_Alloc(2 * vsize * sizeof(*volumeID));
	cellID   = (volumeID) ? volumeID + vsize : 0;
    }

    now = osi_Time();
    for (i=0;i<NVOLS;i++) {
	for (tv = afs_volumes[i]; tv; tv=tv->next) {
	    if (flags & AFS_VOLCHECK_EXPIRED) {
		    if ( ((tv->expireTime<(now+10)) && (tv->states & VRO)) ||
				(flags & AFS_VOLCHECK_FORCE)){
		    afs_ResetVolumeInfo(tv);  /* also resets status */
		    if (volumeID) {
			volumeID[nvols] = tv->volume;
			cellID[nvols]   = tv->cell;
		    }
		    ++nvols;
		    continue;
		}
	    }
	    if (flags & (AFS_VOLCHECK_BUSY | AFS_VOLCHECK_FORCE)) {
		for (j=0; j<MAXHOSTS; j++) 
		    tv->status[j] = not_busy;
	    }

	}
    }
    ReleaseReadLock(&afs_xvolume);


    /* next ensure all mt points are re-evaluated */
    if (nvols || (flags & (AFS_VOLCHECK_FORCE | AFS_VOLCHECK_MTPTS))) {
	ObtainReadLock(&afs_xvcache);
	for(i=0;i<VCSIZE;i++) {
	    for(tvc = afs_vhashT[i]; tvc; tvc=tvc->hnext) {

		/* if the volume of "mvid" of the vcache entry is among the
                 * ones we found earlier, then we re-evaluate it.  Also, if the
                 * force bit is set or we explicitly asked to reevaluate the
                 * mt-pts, we clean the cmvalid bit */

		if ((flags & (AFS_VOLCHECK_FORCE | AFS_VOLCHECK_MTPTS)) ||
		    (tvc->mvid &&
		     inVolList(tvc->mvid, nvols, volumeID, cellID)))
		    tvc->states &= ~CMValid;

		/* If the volume that this file belongs to was reset earlier,
		 * then we should remove its callback.
		 * Again, if forced, always do it.
		 */
		if ((tvc->states & CRO) &&
		    (inVolList(&tvc->fid, nvols, volumeID, cellID) ||
		     (flags & AFS_VOLCHECK_FORCE))) {

		    AFS_FAST_HOLD(tvc);
		    ReleaseReadLock(&afs_xvcache);

		    ObtainWriteLock(&afs_xcbhash, 485);
		    afs_DequeueCallback(tvc);
		    tvc->states &= ~CStatd;
		    ReleaseWriteLock(&afs_xcbhash);
		    if (tvc->fid.Fid.Vnode & 1 || (vType(tvc) == VDIR))
			osi_dnlc_purgedp(tvc);

		    ObtainReadLock(&afs_xvcache);

		    /* our tvc ptr is still good until now */
		    AFS_FAST_RELE(tvc);
		}
	    }
	}
	osi_dnlc_purge ();   /* definitely overkill, but it's safer this way. */
	ReleaseReadLock(&afs_xvcache);
    }

    if (volumeID)
	afs_osi_Free(volumeID, 2 * vsize * sizeof(*volumeID));

} /*afs_CheckVolumeNames*/


static inVolList(fid, nvols, vID, cID)
    struct VenusFid *fid;
    afs_int32  nvols, *vID, *cID;
{
    afs_int32 i;

    /* if no arrays present, be conservative and return true */
    if ( nvols && (!vID || !cID))
	return 1;

    for (i=0; i< nvols; ++i) {
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
struct volume *afs_FindVolume(struct VenusFid *afid, afs_int32 locktype)
{
    struct volume *tv;
    afs_int32 i;

    if (afid == NULL)
	return NULL;

    i = VHash(afid->Fid.Volume);
    ObtainWriteLock(&afs_xvolume,106);
    for (tv = afs_volumes[i]; tv; tv=tv->next) {
	if (tv->volume == afid->Fid.Volume && tv->cell == afid->Cell
	    && (tv->states & VRecheck) == 0) {
	    tv->refCount++;
	    break;
	  }
    }
    ReleaseWriteLock(&afs_xvolume);
    return tv;      /* NULL if we didn't find it */
} /*afs_FindVolume*/



/*
 * Note that areq may be null, in which case we don't bother to set any
 * request status information.
 */
struct volume *afs_GetVolume(afid, areq, locktype)
    struct vrequest *areq;
    struct VenusFid *afid;
    afs_int32 locktype;
{
    struct volume *tv;
    char *bp, tbuf[CVBS];
    AFS_STATCNT(afs_GetVolume);

    tv = afs_FindVolume(afid, locktype);
    if (!tv) {
       bp = afs_cv2string(&tbuf[CVBS], afid->Fid.Volume);
       tv = afs_NewVolumeByName(bp, afid->Cell, 0, areq, locktype);
    }
    return tv;
} /*afs_GetVolume*/



static struct volume *afs_SetupVolume(volid, aname, ve, tcell, agood, type, areq)
    char *aname;
    char *ve;
    afs_int32 volid, agood, type;
    struct cell *tcell;
    struct vrequest *areq;
{
    struct volume *tv;
    struct vldbentry *ove = (struct vldbentry *)ve;
    struct nvldbentry *nve = (struct nvldbentry *)ve;
    struct uvldbentry *uve = (struct uvldbentry *)ve;

    int	whichType;  /* which type of volume to look for */
    int i, j, err=0;

    if (!volid) {
        int len;
	/* special hint from file server to use vlserver */
	len = strlen(aname);
	if (len >= 8 && strcmp(aname+len-7, ".backup") == 0)
	    whichType = BACKVOL;
	else if (len >= 10 && strcmp(aname+len-9, ".readonly")==0)
	    whichType = ROVOL;
	else 
	    whichType = RWVOL;

	/* figure out which one we're really interested in (a set is returned) */
	volid = afs_vtoi(aname);
	if (volid == 0) {
	    if (type == 2) {
		volid = uve->volumeId[whichType];
	    }
	    else if (type == 1) {
		volid = nve->volumeId[whichType];
	    }
	    else {
		volid = ove->volumeId[whichType];
	    }
	}
    }

    
    ObtainWriteLock(&afs_xvolume,108);
    i = VHash(volid);
    for (tv = afs_volumes[i]; tv; tv=tv->next) {
	if (tv->volume == volid && tv->cell == tcell->cell) {
	    break;
	}
    }
    if (!tv) {
	struct fvolume *tf=0;

	tv = afs_GetVolSlot();
	bzero((char *)tv, sizeof(struct volume));
	tv->cell = tcell->cell;
	RWLOCK_INIT(&tv->lock, "volume lock");
	tv->next = afs_volumes[i];	/* thread into list */
	afs_volumes[i] = tv;
	tv->volume = volid;
	for (j=fvTable[FVHash(tv->cell,volid)]; j!=0; j=tf->next) {
	    if (afs_FVIndex != j) {
		char *tfile;
		tfile = osi_UFSOpen(volumeInode);
		err = afs_osi_Read(tfile, sizeof(struct fvolume) * j, &staticFVolume, sizeof(struct fvolume));
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
    tv->states &= ~VRecheck;	    /* just checked it */
    tv->accessTime = osi_Time();
    ReleaseWriteLock(&afs_xvolume);
    ObtainWriteLock(&tv->lock,111);
    if (type == 2) {
	InstallUVolumeEntry(tv, uve, tcell->cell, tcell, areq);
    }
    else if (type == 1)
	InstallNVolumeEntry(tv, nve, tcell->cell);
    else
	InstallVolumeEntry(tv, ove, tcell->cell);
    if (agood) {
	if (!tv->name) {
	    tv->name = afs_osi_Alloc(strlen(aname) + 1);
	    strcpy(tv->name, aname);
	}
    }
    for (i=0; i<NMAXNSERVERS; i++) {
       tv->status[i] = not_busy;
    }
    ReleaseWriteLock(&tv->lock);
    return tv;
}


struct volume *afs_GetVolumeByName(aname, acell, agood, areq, locktype)
    struct vrequest *areq;
    afs_int32 acell;
    int agood;
    register char *aname;
    afs_int32 locktype;
{
  afs_int32 i;
  struct volume *tv;

    AFS_STATCNT(afs_GetVolumeByName);
    ObtainWriteLock(&afs_xvolume,112);
  for (i=0;i<NVOLS;i++) {
	for (tv = afs_volumes[i]; tv; tv=tv->next) {
	    if (tv->name && !strcmp(aname,tv->name) && tv->cell == acell
		&& (tv->states&VRecheck) == 0) {
		tv->refCount++;
		ReleaseWriteLock(&afs_xvolume);
		return tv;
	    }
	}
    }

    ReleaseWriteLock(&afs_xvolume);

  tv = afs_NewVolumeByName(aname, acell, agood, areq, locktype);
  return(tv);
}

int lastnvcode;
static struct volume *afs_NewVolumeByName(char *aname, afs_int32 acell, int agood,
				   struct vrequest *areq, afs_int32 locktype)
{
    afs_int32 code, i, type=0;
    struct volume *tv, *tv1;
    struct vldbentry *tve;
    struct nvldbentry *ntve;
    struct uvldbentry *utve;
    struct cell *tcell;
    char *tbuffer, *ve;
    struct conn *tconn;
    struct vrequest treq;
    XSTATS_DECLS;

    if (strlen(aname) > VL_MAXNAMELEN)	/* Invalid volume name */
	return (struct volume *) 0;

    tcell = afs_GetCell(acell, READ_LOCK);
    if (!tcell) {
	return (struct volume *) 0;
    }

    /* allow null request if we don't care about ENODEV/ETIMEDOUT distinction */
    if (!areq) 	areq = &treq;


    afs_Trace2(afs_iclSetp, CM_TRACE_GETVOL, ICL_TYPE_STRING, aname,
			   ICL_TYPE_POINTER, aname);
    tbuffer = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    tve = (struct vldbentry *) (tbuffer+1024);
    ntve = (struct nvldbentry *)tve;
    utve = (struct uvldbentry *)tve;
    afs_InitReq(&treq, &afs_osi_cred);	/* *must* be unauth for vldb */
    do {
	tconn = afs_ConnByMHosts(tcell->cellHosts, tcell->vlport,
				 tcell->cell, &treq, SHARED_LOCK);
	if (tconn) {
	    if (tconn->srvr->server->flags & SNO_LHOSTS) {
		type = 0;
#ifdef RX_ENABLE_LOCKS
		AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		code = VL_GetEntryByNameO(tconn->id, aname, tve);
#ifdef RX_ENABLE_LOCKS
		AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	    } else if (tconn->srvr->server->flags & SYES_LHOSTS) {
		type = 1;
#ifdef RX_ENABLE_LOCKS
		AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		code = VL_GetEntryByNameN(tconn->id, aname, ntve);
#ifdef RX_ENABLE_LOCKS
		AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	    } else {
		type = 2;
#ifdef RX_ENABLE_LOCKS
		AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		code = VL_GetEntryByNameU(tconn->id, aname, utve);
#ifdef RX_ENABLE_LOCKS
		AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
		if (!(tconn->srvr->server->flags & SVLSRV_UUID)) {
		    if (code == RXGEN_OPCODE) {
			type = 1;
#ifdef RX_ENABLE_LOCKS
			AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
                        code = VL_GetEntryByNameN(tconn->id, aname, ntve);
#ifdef RX_ENABLE_LOCKS
			AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
			if (code == RXGEN_OPCODE) {
			    type = 0;
			    tconn->srvr->server->flags |= SNO_LHOSTS;
#ifdef RX_ENABLE_LOCKS
			    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
			    code = VL_GetEntryByNameO(tconn->id, aname, tve);
#ifdef RX_ENABLE_LOCKS
			    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
			} else if (!code)
			    tconn->srvr->server->flags |= SYES_LHOSTS;
		    } else if (!code)
			    tconn->srvr->server->flags |= SVLSRV_UUID;
		}
		lastnvcode = code;
	    }
	} else
	    code = -1;
    } while
      (afs_Analyze(tconn, code, (struct VenusFid *) 0, areq,
		   -1, /* no op code for this */
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
	    for (i=0; i<MAXCELLHOSTS; i++) {
		if ((sp = tcell->cellHosts[i]) == (struct server *) 0) break;
		for (sap = sp->addr; sap; sap = sap->next_sa)
		    afs_MarkServerUpOrDown(sap, 0);
	    }
	}
	afs_CopyError(&treq, areq);
	osi_FreeLargeSpace(tbuffer);
	afs_PutCell(tcell, READ_LOCK);
	return (struct volume *) 0;
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
    } /*First time a volume in this cell has been referenced*/

    if (type == 2)
	ve = (char *)utve;
    else if (type == 1)
	ve = (char *)ntve;
    else
	ve = (char *)tve;
    tv = afs_SetupVolume(0, aname, ve, tcell, agood, type, areq);
    if ((agood == 2) && tv->roVol) {
	/*
	 * This means that very soon we'll ask for the RO volume so
	 * we'll prefetch it (well we did already.)
	 */
	tv1 = afs_SetupVolume(tv->roVol, (char *)0, ve, tcell, 0, type, areq);
	tv1->refCount--;
    }
    osi_FreeLargeSpace(tbuffer);
    afs_PutCell(tcell, READ_LOCK);
    return tv;

} /*afs_NewVolumeByName*/



/* call this with the volume structure locked; used for new-style vldb requests */
void InstallVolumeEntry(struct volume *av, struct vldbentry *ve, int acell)
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
    }
    else if ((ve->flags & VLF_ROEXISTS) && (av->volume == ve->volumeId[ROVOL])) {
       mask = VLSF_ROVOL;
       av->states |= VRO;
    }
    else if ((ve->flags & VLF_BACKEXISTS) && (av->volume == ve->volumeId[BACKVOL])) {
       /* backup always is on the same volume as parent */
       mask = VLSF_RWVOL;
       av->states |= (VRO|VBackup);
    }
    else {
       mask = 0;  /* Can't find volume in vldb entry */
    }

    /* fill in volume types */
    av->rwVol   = ((ve->flags & VLF_RWEXISTS)   ? ve->volumeId[RWVOL]   : 0);
    av->roVol   = ((ve->flags & VLF_ROEXISTS)   ? ve->volumeId[ROVOL]   : 0);
    av->backVol = ((ve->flags & VLF_BACKEXISTS) ? ve->volumeId[BACKVOL] : 0);

    if (ve->flags & VLF_DFSFILESET)
	av->states |= VForeign;

    cellp = afs_GetCell(acell, 0);

    /* Step through the VLDB entry making sure each server listed is there */
    for (i=0,j=0; i<ve->nServers; i++) {
	if ( ((ve->serverFlags[i] & mask) == 0) || (ve->serverFlags[i] & VLSF_DONTUSE) ) {
	   continue;      /* wrong volume or  don't use this volume */
	}

	temp = htonl(ve->serverNumber[i]);
	ts = afs_GetServer(&temp, 1, acell, cellp->fsport, WRITE_LOCK, (afsUUID *)0, 0);
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
} /*InstallVolumeEntry*/


void InstallNVolumeEntry(struct volume *av, struct nvldbentry *ve, int acell)
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
    }
    else if ((ve->flags & VLF_ROEXISTS) && (av->volume == ve->volumeId[ROVOL])) {
       mask = VLSF_ROVOL;
       av->states |= VRO;
    }
    else if ((ve->flags&VLF_BACKEXISTS) && (av->volume == ve->volumeId[BACKVOL])) {
       /* backup always is on the same volume as parent */
       mask = VLSF_RWVOL;
       av->states |= (VRO|VBackup);
    }
    else {
       mask = 0;     /* Can't find volume in vldb entry */
    }

    /* fill in volume types */
    av->rwVol   = ((ve->flags & VLF_RWEXISTS)   ? ve->volumeId[RWVOL]   : 0);
    av->roVol   = ((ve->flags & VLF_ROEXISTS)   ? ve->volumeId[ROVOL]   : 0);
    av->backVol = ((ve->flags & VLF_BACKEXISTS) ? ve->volumeId[BACKVOL] : 0);

    if (ve->flags & VLF_DFSFILESET)
	av->states |= VForeign;

    cellp = afs_GetCell(acell, 0);

    /* Step through the VLDB entry making sure each server listed is there */
    for (i=0,j=0; i<ve->nServers; i++) {
        if ( ((ve->serverFlags[i] & mask) == 0) || (ve->serverFlags[i] & VLSF_DONTUSE) ) {
	   continue;      /* wrong volume or don't use this volume */
	}

	temp = htonl(ve->serverNumber[i]);
	ts = afs_GetServer(&temp, 1, acell, cellp->fsport, WRITE_LOCK, (afsUUID *)0,0);
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
} /*InstallNVolumeEntry*/


void InstallUVolumeEntry(struct volume *av, struct uvldbentry *ve,
			 int acell, struct cell *tcell, struct vrequest *areq)
{
    register struct srvAddr *sa;
    register struct server *ts;
    struct conn *tconn;
    struct cell *cellp;
    register int i, j;
    afs_uint32 serverid;
    afs_int32 mask;
    int hash, k;

    AFS_STATCNT(InstallVolumeEntry);

    /* Determine type of volume we want */
    if ((ve->flags & VLF_RWEXISTS) && (av->volume == ve->volumeId[RWVOL])) {
       mask = VLSF_RWVOL;
    }
    else if ((ve->flags & VLF_ROEXISTS) && av->volume == ve->volumeId[ROVOL]) {
       mask = VLSF_ROVOL;
       av->states |= VRO;
    }
    else if ((ve->flags & VLF_BACKEXISTS) && (av->volume == ve->volumeId[BACKVOL])) {
       /* backup always is on the same volume as parent */
       mask = VLSF_RWVOL;
       av->states |= (VRO|VBackup);
    }
    else {
       mask = 0;      /* Can't find volume in vldb entry */
    }

    /* fill in volume types */
    av->rwVol   = ((ve->flags & VLF_RWEXISTS)   ? ve->volumeId[RWVOL]   : 0);
    av->roVol   = ((ve->flags & VLF_ROEXISTS)   ? ve->volumeId[ROVOL]   : 0);
    av->backVol = ((ve->flags & VLF_BACKEXISTS) ? ve->volumeId[BACKVOL] : 0);

    if (ve->flags & VLF_DFSFILESET)
	av->states |= VForeign;

    cellp = afs_GetCell(acell, 0);

    /* This volume, av, is locked. Zero out the serverHosts[] array 
     * so that if afs_GetServer() decides to replace the server 
     * struct, we don't deadlock trying to afs_ResetVolumeInfo()
     * this volume.
     */
    for (j=0; j<MAXHOSTS; j++) {
       av->serverHost[j] = 0;
    }

    /* Gather the list of servers the VLDB says the volume is on
     * and initialize the ve->serverHost[] array. If a server struct
     * is not found, then get the list of addresses for the
     * server, VL_GetAddrsU(), and create a server struct, afs_GetServer().
     */
    for (i=0,j=0; i<ve->nServers; i++) {
        if ( ((ve->serverFlags[i] & mask) == 0) || (ve->serverFlags[i] & VLSF_DONTUSE) ) {
	   continue;       /* wrong volume don't use this volume */
	}

	if (!(ve->serverFlags[i] & VLSERVER_FLAG_UUID)) {
	    /* The server has no uuid */
	    serverid = htonl(ve->serverNumber[i].time_low);
	    ts = afs_GetServer(&serverid, 1, acell, cellp->fsport, WRITE_LOCK, (afsUUID *)0,0);
	} else {
	    ts = afs_FindServer(0, cellp->fsport, &ve->serverNumber[i], 0);
	    if (ts && (ts->sr_addr_uniquifier == ve->serverUnique[i]) && ts->addr) {
	       /* uuid, uniquifier, and portal are the same */
	    } else {
		afs_uint32 *addrp, nentries, code, unique;
		bulkaddrs addrs;
		ListAddrByAttributes attrs;
		afsUUID uuid;

		bzero((char *)&attrs, sizeof(attrs));
		attrs.Mask = VLADDR_UUID;
		attrs.uuid = ve->serverNumber[i];
		bzero((char *)&uuid, sizeof(uuid));
		bzero((char *)&addrs, sizeof(addrs));
		do {
		    tconn = afs_ConnByMHosts(tcell->cellHosts, tcell->vlport,
					     tcell->cell, areq, SHARED_LOCK);
		    if (tconn) {
#ifdef RX_ENABLE_LOCKS
			AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
			code = VL_GetAddrsU(tconn->id, &attrs, &uuid, &unique, &nentries, &addrs);
#ifdef RX_ENABLE_LOCKS
			AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
		    } else {
			code = -1;
		    }

		    /* Handle corrupt VLDB (defect 7393) */
		    if (code == 0 && nentries == 0)
			code = VL_NOENT;

		} while (afs_Analyze(tconn, code, (struct VenusFid *) 0, areq,
				     -1, SHARED_LOCK, tcell));
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
		afs_osi_Free(addrs.bulkaddrs_val, addrs.bulkaddrs_len*sizeof(*addrp));
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
} /*InstallVolumeEntry*/


void afs_ResetVolumeInfo(struct volume *tv)
{
    int i;

    AFS_STATCNT(afs_ResetVolumeInfo);
    ObtainWriteLock(&tv->lock,117);
    tv->states |= VRecheck;
    for (i=0; i<MAXHOSTS; i++)
      tv->status[i] = not_busy;
    if (tv->name) {
	afs_osi_Free(tv->name, strlen(tv->name)+1);
	tv->name = (char *) 0;
      }
    ReleaseWriteLock(&tv->lock);
} /*afs_ResetVolumeInfo*/
