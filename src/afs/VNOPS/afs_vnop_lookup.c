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
 * afs_lookup
 * EvalMountPoint
 * afs_DoBulkStat
 *
 * Locals:
 * afs_strcat
 * AFS_EQ_ATSYS (macro)
 * afs_index
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h" /* statistics */
#include "../afs/afs_cbqueue.h"
#include "../afs/nfsclient.h"
#include "../afs/exporter.h"
#include "../afs/afs_osidnlc.h"


/**
 * A few definitions. This is until we have a proper header file	
 * which has prototypes for all functions
 */

extern struct DirEntry * afs_dir_GetBlob();

extern afs_rwlock_t afs_xvcache;
extern afs_rwlock_t afs_xcbhash;
extern struct afs_exporter *afs_nfsexporter;
extern char *afs_sysname;
extern struct afs_q VLRU;			/*vcache LRU*/
#ifdef AFS_LINUX22_ENV
extern struct inode_operations afs_symlink_iops, afs_dir_iops;
#endif


afs_int32 afs_bulkStatsDone;
static int bulkStatCounter = 0;	/* counter for bulk stat seq. numbers */


/* this would be faster if it did comparison as int32word, but would be 
 * dependant on byte-order and alignment, and I haven't figured out
 * what "@sys" is in binary... */
#define AFS_EQ_ATSYS(name) (((name)[0]=='@')&&((name)[1]=='s')&&((name)[2]=='y')&&((name)[3]=='s')&&(!(name)[4]))

char *
afs_strcat(s1, s2)
	register char *s1, *s2;
{
	register char *os1;

	AFS_STATCNT(strcat);
	os1 = s1;
	while (*s1++)
		;
	--s1;
	while (*s1++ = *s2++)
		;
	return (os1);
}


char *afs_index(a, c)
    register char *a, c; {
    register char tc;
    AFS_STATCNT(afs_index);
    while (tc = *a) {
	if (tc == c) return a;
	else a++;
    }
    return (char *) 0;
}

/* call under write lock, evaluate mvid field from a mt pt.
 * avc is the vnode of the mount point object.
 * advc is the vnode of the containing directory
 * avolpp is where we return a pointer to the volume named by the mount pt, if success
 * areq is the identity of the caller.
 *
 * NOTE: this function returns a held volume structure in *volpp if it returns 0!
 */
EvalMountPoint(avc, advc, avolpp, areq)
    register struct vcache *avc;
    struct volume **avolpp;
    struct vcache *advc;	    /* the containing dir */
    register struct vrequest *areq;
{
    afs_int32  code;
    struct volume *tvp = 0;
    struct VenusFid tfid;
    struct cell *tcell;
    char   *cpos, *volnamep;
    char   type, buf[128];
    afs_int32  prefetchRO;          /* 1=>No  2=>Yes */
    afs_int32  mtptCell, assocCell, hac=0;
    afs_int32  samecell, roname, len;

    AFS_STATCNT(EvalMountPoint);
#ifdef notdef
    if (avc->mvid && (avc->states & CMValid)) return 0;	/* done while racing */
#endif
    *avolpp = (struct volume *)0;
    code = afs_HandleLink(avc, areq);
    if (code) return code;

    /* Determine which cell and volume the mointpoint goes to */
    type = avc->linkData[0];                   /* '#'=>Regular '%'=>RW */
    cpos = afs_index(&avc->linkData[1], ':');  /* if cell name present */
    if (cpos) {
       volnamep = cpos+1;
       *cpos = 0;
       tcell = afs_GetCellByName(&avc->linkData[1], READ_LOCK);
       *cpos =	':';
    } else {
       volnamep = &avc->linkData[1];
       tcell = afs_GetCell(avc->fid.Cell, READ_LOCK);
    }
    if (!tcell) return ENODEV;

    mtptCell = tcell->cell;               /* The cell for the mountpoint */
    if (tcell->lcellp) {
       hac = 1;                           /* has associated cell */
       assocCell = tcell->lcellp->cell;   /* The associated cell */
    }
    afs_PutCell(tcell, READ_LOCK);	    

    /* Is volume name a "<n>.backup" or "<n>.readonly" name */
    len = strlen(volnamep);
    roname = ((len > 9) && (strcmp(&volnamep[len - 9],".readonly") == 0)) ||
             ((len > 7) && (strcmp(&volnamep[len - 7],".backup")   == 0));

    /* When we cross mountpoint, do we stay in the same cell */
    samecell = (avc->fid.Cell == mtptCell) || (hac && (avc->fid.Cell == assocCell));

    /* Decide whether to prefetch the RO. Also means we want the RO.
     * If this is a regular mountpoint with a RW volume name and
     * we cross a cell boundary -or- start from a RO volume, then we will
     * want to prefetch the RO volume when we get the RW below.
     */
    if ( (type == '#') && !roname && (!samecell || (avc->states & CRO)) ) {
       prefetchRO = 2; /* Yes, prefetch the RO */
    } else {
       prefetchRO = 1; /* No prefetch of the RO */
    }

    /* Get the volume struct. Unless this volume name has ".readonly" or
     * ".backup" in it, this will get the volume struct for the RW volume.
     * The RO volume will be prefetched if requested (but not returned).
     */
    tvp = afs_GetVolumeByName(volnamep, mtptCell, prefetchRO, areq, WRITE_LOCK);

    /* If no volume was found in this cell, try the associated linked cell */
    if (!tvp && hac && areq->volumeError) {
       tvp = afs_GetVolumeByName(volnamep, assocCell, prefetchRO, areq, WRITE_LOCK);
    }

    /* Still not found. If we are looking for the RO, then perhaps the RW 
     * doesn't exist? Try adding ".readonly" to volname and look for that.
     * Don't know why we do this. Would have still found it in above call - jpm.
     */
    if (!tvp && (prefetchRO == 2)) {
       strcpy(buf, volnamep);
       afs_strcat(buf, ".readonly");

       tvp = afs_GetVolumeByName(buf, mtptCell, 1, areq, WRITE_LOCK);

       /* Try the associated linked cell if failed */
       if (!tvp && hac && areq->volumeError) {
	  tvp = afs_GetVolumeByName(buf, assocCell, 1, areq, WRITE_LOCK);
       }
    }
  
    if (!tvp) return ENOENT;       /* Couldn't find the volume */

    /* Don't cross mountpoint from a BK to a BK volume */
    if ((avc->states & CBackup) && (tvp->states & VBackup)) {
	afs_PutVolume(tvp, WRITE_LOCK);
	return ELOOP;
    }

    /* If we want (prefetched) the RO and it exists, then drop the
     * RW volume and get the RO. Othewise, go with the RW.
     */
    if ((prefetchRO == 2) && tvp->roVol) {
       tfid.Fid.Volume = tvp->roVol;                 /* remember RO volume */
       tfid.Cell       = tvp->cell;
       afs_PutVolume(tvp, WRITE_LOCK);               /* release old volume */
       tvp = afs_GetVolume(&tfid, areq, WRITE_LOCK); /* get the new one */
       if (!tvp) return ENOENT;                      /* oops, can't do it */
    }

    if (avc->mvid == 0)
	avc->mvid = (struct VenusFid *) osi_AllocSmallSpace(sizeof(struct VenusFid));
    avc->mvid->Cell = tvp->cell;
    avc->mvid->Fid.Volume = tvp->volume;
    avc->mvid->Fid.Vnode = 1;
    avc->mvid->Fid.Unique = 1;
    avc->states |= CMValid;

    /* Used to: if the mount point is stored within a backup volume,
     * then we should only update the parent pointer information if
     * there's none already set, so as to avoid updating a volume's ..
     * info with something in an OldFiles directory.
     *
     * Next two lines used to be under this if:
     *
     * if (!(avc->states & CBackup) || tvp->dotdot.Fid.Volume == 0)
     *
     * Now: update mount point back pointer on every call, so that we handle
     * multiple mount points better.  This way, when du tries to go back
     * via chddir(".."), it will end up exactly where it started, yet
     * cd'ing via a new path to a volume will reset the ".." pointer
     * to the new path.
     */
    tvp->mtpoint = avc->fid;	/* setup back pointer to mtpoint */
    tvp->dotdot  = advc->fid;

    *avolpp = tvp;
    return 0;
}
    
afs_ENameOK(aname)
    register char *aname; {
    register char tc;
    register int tlen;

    AFS_STATCNT(ENameOK);
    tlen = strlen(aname);
    if (tlen >= 4 && strcmp(aname+tlen-4, "@sys") == 0) return 0;
    return 1;
}

Check_AtSys(avc, aname, outb, areq)
    register struct vcache *avc;
    char *aname, **outb;
    struct vrequest *areq;
{
    register char *tname;
    register int error = 0, offset = -1;

    for (tname=aname; *tname; tname++) /*Move to the end of the string*/;

    /*
     * If the current string is 4 chars long or more, check to see if the
     * tail end is "@sys".
     */
    if ((tname >= aname + 4) && (AFS_EQ_ATSYS(tname-4)))
	offset = (tname - 4) - aname;
    if (offset < 0) {
	tname = aname;
    }  else {
	tname = (char *) osi_AllocLargeSpace(AFS_LRALLOCSIZ);
	if (offset)
	    strncpy(tname, aname, offset);
	if (!afs_nfsexporter) 
	    strcpy(tname+offset, (afs_sysname ? afs_sysname : SYS_NAME ));
	else {
	    register struct unixuser *au;
	    register afs_int32 error;
	    au = afs_GetUser(areq->uid, avc->fid.Cell, 0); afs_PutUser(au, 0);	
	    if (au->exporter) {
		error = EXP_SYSNAME(au->exporter, (char *)0, tname+offset);
		if (error) 
		    strcpy(tname+offset, "@sys");
	    } else {
		strcpy(tname+offset, (afs_sysname ? afs_sysname : SYS_NAME ));
	    }
	}
	error = 1;
    }
    *outb = tname;
    return error;
}


char *afs_getsysname(areq, adp)
    register struct vrequest *areq;
    register struct vcache *adp; {
    static char sysname[MAXSYSNAME];
    register struct unixuser *au;
    register afs_int32 error;

    AFS_STATCNT(getsysname);
    /* this whole interface is wrong, it should take a buffer ptr and copy
     * the data out.
     */
    au = afs_GetUser(areq->uid, adp->fid.Cell, 0);
    afs_PutUser(au, 0);	
    if (au->exporter) {
      error = EXP_SYSNAME(au->exporter, (char *)0, sysname);
      if (error) return "@sys";
      else return sysname;
    } else {
      return (afs_sysname == 0? SYS_NAME : afs_sysname);
    }
}

void afs_HandleAtName(aname, aresult, areq, adp)
    register char *aname;
    register char *aresult;
    register struct vrequest *areq;
    register struct vcache *adp; {
    register int tlen;
    AFS_STATCNT(HandleAtName);
    tlen = strlen(aname);
    if (tlen >= 4 && strcmp(aname+tlen-4, "@sys")==0) {
	strncpy(aresult, aname, tlen-4);
	strcpy(aresult+tlen-4, afs_getsysname(areq, adp));
    }
    else strcpy(aresult, aname);
    }

#if (defined(AFS_SGI62_ENV) || defined(AFS_SUN57_64BIT_ENV))
extern int BlobScan(ino64_t *afile, afs_int32 ablob);
#else
extern int BlobScan(afs_int32 *afile, afs_int32 ablob);
#endif


/* called with an unlocked directory and directory cookie.  Areqp
 * describes who is making the call.
 * Scans the next N (about 30, typically) directory entries, and does
 * a bulk stat call to stat them all.
 *
 * Must be very careful when merging in RPC responses, since we dont
 * want to overwrite newer info that was added by a file system mutating
 * call that ran concurrently with our bulk stat call.
 *
 * We do that, as described below, by not merging in our info (always
 * safe to skip the merge) if the status info is valid in the vcache entry.
 *
 * If adapt ever implements the bulk stat RPC, then this code will need to
 * ensure that vcaches created for failed RPC's to older servers have the
 * CForeign bit set.
 */
struct vcache * BStvc = (struct vcache *) 0;
void afs_DoBulkStat(adp, dirCookie, areqp)
  struct vcache *adp;
  long dirCookie;
  struct vrequest *areqp;
{
    int nentries;		/* # of entries to prefetch */
    int nskip;			/* # of slots in the LRU queue to skip */
    struct vcache *lruvcp;	/* vcache ptr of our goal pos in LRU queue */
    struct dcache *dcp;		/* chunk containing the dir block */
    char *statMemp;		/* status memory block */
    char *cbfMemp;		/* callback and fid memory block */
    long temp;			/* temp for holding chunk length, &c. */
    struct AFSFid *fidsp;	/* file IDs were collecting */
    struct AFSCallBack *cbsp;	/* call back pointers */
    struct AFSCallBack *tcbp;	/* temp callback ptr */
    struct AFSFetchStatus *statsp;	/* file status info */
    struct AFSVolSync volSync;	/* vol sync return info */
    struct vcache *tvcp;	/* temp vcp */
    struct afs_q *tq;		/* temp queue variable */
    AFSCBFids fidParm;		/* file ID parm for bulk stat */
    AFSBulkStats statParm;	/* stat info parm for bulk stat */
    int fidIndex;		/* which file were stating */
    struct conn *tcp;		/* conn for call */
    AFSCBs cbParm;		/* callback parm for bulk stat */
    struct server *hostp = 0;	/* host we got callback from */
    long origEvenCBs;		/* original # of callbacks for even-fid files */
    long origOddCBs;		/* original # of callbacks for odd-fid files */
    long origEvenZaps;		/* original # of recycles for even-fid files */
    long origOddZaps;		/* original # of recycles for odd-fid files */
    long startTime;		/* time we started the call,
				 * for callback expiration base
				 */
    int statSeqNo;		/* Valued of file size to detect races */
    int code;			/* error code */
    long newIndex;		/* new index in the dir */
    struct DirEntry *dirEntryp;	/* dir entry we are examining */
    int i;
    struct VenusFid afid;	/* file ID we are using now */
    struct VenusFid tfid;	/* another temp. file ID */
    afs_int32 retry;                  /* handle low-level SGI MP race conditions */
    long volStates;		/* flags from vol structure */
    struct volume *volp=0;	/* volume ptr */
    struct VenusFid dotdot;
    int	flagIndex;		/* First file with bulk fetch flag set */
    XSTATS_DECLS

    /* first compute some basic parameters.  We dont want to prefetch more
     * than a fraction of the cache in any given call, and we want to preserve
     * a portion of the LRU queue in any event, so as to avoid thrashing
     * the entire stat cache (we will at least leave some of it alone).
     * presently dont stat more than 1/8 the cache in any one call.      */
    nentries = afs_cacheStats / 8;

    /* dont bother prefetching more than one calls worth of info */
    if (nentries > AFSCBMAX) nentries = AFSCBMAX;

    /* heuristic to make sure that things fit in 4K.  This means that
     * we shouldnt make it any bigger than 47 entries.  I am typically
     * going to keep it a little lower, since we don't want to load
     * too much of the stat cache.
     */
    if (nentries > 30) nentries = 30;

    /* now, to reduce the stack size, well allocate two 4K blocks,
     * one for fids and callbacks, and one for stat info.  Well set
     * up our pointers to the memory from there, too.
     */
    statMemp = osi_AllocLargeSpace(nentries * sizeof(AFSFetchStatus));
    statsp = (struct AFSFetchStatus *) statMemp;
    cbfMemp = osi_AllocLargeSpace(nentries *
	(sizeof(AFSCallBack) + sizeof(AFSFid)));
    fidsp = (AFSFid *) cbfMemp;
    cbsp = (AFSCallBack *) (cbfMemp + nentries * sizeof(AFSFid));

    /* next, we must iterate over the directory, starting from the specified
     * cookie offset (dirCookie), and counting out nentries file entries.
     * We skip files that already have stat cache entries, since we
     * dont want to bulk stat files that are already in the cache.
     */
tagain:
    code = afs_VerifyVCache(adp, areqp);
    if (code) goto done;

    dcp = afs_GetDCache(adp, 0, areqp, &temp, &temp, 1);
    if (!dcp) {
	code = ENOENT;
	goto done;
    }

    /* lock the directory cache entry */
    ObtainReadLock(&adp->lock);

    /*
     * Make sure that the data in the cache is current. There are two
     * cases we need to worry about:
     * 1. The cache data is being fetched by another process.
     * 2. The cache data is no longer valid
     */
    while ((adp->states & CStatd)
	   && (dcp->flags & DFFetching)
	   && hsame(adp->m.DataVersion, dcp->f.versionNo)) {
	dcp->flags |= DFWaiting;
	ReleaseReadLock(&adp->lock);
	afs_osi_Sleep(&dcp->validPos);
	ObtainReadLock(&adp->lock);
    }
    if (!(adp->states & CStatd)
	|| !hsame(adp->m.DataVersion, dcp->f.versionNo)) {
	ReleaseReadLock(&adp->lock);
	afs_PutDCache(dcp);
	goto tagain;
    }

    /* Generate a sequence number so we can tell whether we should
     * store the attributes when processing the response. This number is
     * stored in the file size when we set the CBulkFetching bit. If the
     * CBulkFetching is still set and this value hasn't changed, then
     * we know we were the last to set CBulkFetching bit for this file,
     * and it is safe to set the status information for this file.
     */
    statSeqNo = bulkStatCounter++;

    /* now we have dir data in the cache, so scan the dir page */
    fidIndex = 0;
    flagIndex = 0;
    while (1) { /* Should probably have some constant bound */
	/* look for first safe entry to examine in the directory.  BlobScan
	 * looks for a the 1st allocated dir after the dirCookie slot.
	 */
	newIndex = BlobScan(&dcp->f.inode, (dirCookie>>5));
	if (newIndex == 0) break;

	/* remember the updated directory cookie */
	dirCookie = newIndex << 5;

	/* get a ptr to the dir entry */
	dirEntryp =(struct DirEntry *)afs_dir_GetBlob(&dcp->f.inode, newIndex);
	if (!dirEntryp) break;

	/* dont copy more than we have room for */
	if (fidIndex >= nentries) {
	  DRelease((char *) dirEntryp, 0);
	  break;
	}

	/* now, if the dir entry looks good, copy it out to our list.  Vnode
	 * 0 means deleted, although it should also be free were it deleted.
	 */
	if (dirEntryp->fid.vnode != 0) {
	    /* dont copy entries we have in our cache.  This check will
	     * also make us skip "." and probably "..", unless it has
	     * disappeared from the cache since we did our namei call.
	     */
	    tfid.Cell = adp->fid.Cell;
	    tfid.Fid.Volume = adp->fid.Fid.Volume;
	    tfid.Fid.Vnode = ntohl(dirEntryp->fid.vnode);
	    tfid.Fid.Unique = ntohl(dirEntryp->fid.vunique);
	    do {
	      retry = 0;
	      ObtainWriteLock(&afs_xvcache, 130);
	      tvcp = afs_FindVCache(&tfid, 0, 0, &retry, 0 /* no stats | LRU */);
	      if (tvcp && retry) {
		ReleaseWriteLock(&afs_xvcache);
		afs_PutVCache(tvcp);
	      }
	    } while (tvcp && retry);
	    if (!tvcp) {          /* otherwise, create manually */
	      tvcp = afs_NewVCache(&tfid, hostp, 0, 0);
	      ObtainWriteLock(&tvcp->lock, 505);
	      ReleaseWriteLock(&afs_xvcache);
	      afs_RemoveVCB(&tfid);
	      ReleaseWriteLock(&tvcp->lock);
	    } else {
	      ReleaseWriteLock(&afs_xvcache);
	    }
	    if (!tvcp)
	      goto done; /* can't happen at present, more's the pity */

	    /* WARNING: afs_DoBulkStat uses the Length field to store a
	     * sequence number for each bulk status request. Under no
	     * circumstances should afs_DoBulkStat store a sequence number
	     * if the new length will be ignored when afs_ProcessFS is
	     * called with new stats. */
#ifdef AFS_SGI_ENV
	    if (!(tvcp->states & (CStatd|CBulkFetching))
                && (tvcp->execsOrWriters <= 0)
                && !afs_DirtyPages(tvcp)
                && !AFS_VN_MAPPED((vnode_t*)tvcp))
#else
	    if (!(tvcp->states & (CStatd|CBulkFetching))
                && (tvcp->execsOrWriters <= 0) 
                && !afs_DirtyPages(tvcp))
#endif

	    {
		/* this entry doesnt exist in the cache, and is not
		 * already being fetched by someone else, so add it to the
		 * list of file IDs to obtain.
		 *
		 * We detect a callback breaking race condition by checking the
		 * CBulkFetching state bit and the value in the file size.
		 * It is safe to set the status only if the CBulkFetching
		 * flag is still set and the value in the file size does
		 * not change.
		 *
		 * Don't fetch status for dirty files. We need to
		 * preserve the value of the file size. We could
		 * flush the pages, but it wouldn't be worthwhile.
		 */
		bcopy((char *) &tfid.Fid, (char *)(fidsp+fidIndex),
		      sizeof(*fidsp));
		tvcp->states |= CBulkFetching;
		tvcp->m.Length = statSeqNo;
		fidIndex++;
	    }
	    afs_PutVCache(tvcp);
	}	/* if dir vnode has non-zero entry */

	/* move to the next dir entry by adding in the # of entries
	 * used by this dir entry.
	 */
	temp = afs_dir_NameBlobs(dirEntryp->name) << 5;
	DRelease((char *) dirEntryp, 0);
	if (temp <= 0) break;
	dirCookie += temp;
    }	/* while loop over all dir entries */

    /* now release the dir lock and prepare to make the bulk RPC */
    ReleaseReadLock(&adp->lock);

    /* release the chunk */
    afs_PutDCache(dcp);

    /* dont make a null call */
    if (fidIndex == 0) goto done;

    do {
	/* setup the RPC parm structures */
	fidParm.AFSCBFids_len = fidIndex;
	fidParm.AFSCBFids_val = fidsp;
	statParm.AFSBulkStats_len = fidIndex;
	statParm.AFSBulkStats_val = statsp;
	cbParm.AFSCBs_len = fidIndex;
	cbParm.AFSCBs_val = cbsp;

	/* start the timer; callback expirations are relative to this */
	startTime = osi_Time();

	tcp = afs_Conn(&adp->fid, areqp, SHARED_LOCK);
	if (tcp) {
	    hostp = tcp->srvr->server;
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_BULKSTATUS);
#ifdef RX_ENABLE_LOCKS
	    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	    code = RXAFS_BulkStatus(tcp->id, &fidParm, &statParm, &cbParm,
				    &volSync);
#ifdef RX_ENABLE_LOCKS
	    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	    XSTATS_END_TIME;
	}
	else code = -1;
    } while (afs_Analyze(tcp, code, &adp->fid, areqp, 
			 AFS_STATS_FS_RPCIDX_BULKSTATUS, SHARED_LOCK, (struct cell *)0));

    /* now, if we didnt get the info, bail out. */
    if (code) goto done;

    /* we need vol flags to create the entries properly */
    dotdot.Fid.Volume = 0;
    volp = afs_GetVolume(&adp->fid, areqp, READ_LOCK);
    if (volp) {
	volStates = volp->states;
	if (volp->dotdot.Fid.Volume != 0)
	    dotdot = volp->dotdot;
    }
    else volStates = 0;

    /* find the place to merge the info into  We do this by skipping
     * nskip entries in the LRU queue.  The more we skip, the more
     * we preserve, since the head of the VLRU queue is the most recently
     * referenced file.
     */
  reskip:
    nskip = afs_cacheStats / 2;		/* preserved fraction of the cache */
    ObtainReadLock(&afs_xvcache);
    if (QEmpty(&VLRU)) {
      /* actually a serious error, probably should panic. Probably will 
       * panic soon, oh well. */
      ReleaseReadLock(&afs_xvcache);
      goto done;
    }
    if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
       refpanic ("Bulkstat VLRU inconsistent");
    }
    for(tq = VLRU.next; tq != &VLRU; tq = QNext(tq)) {
	if (--nskip <= 0) break;
	else if (QNext(QPrev(tq)) != tq) {
	   BStvc = QTOV(tq);
	   refpanic ("BulkStat VLRU inconsistent");
	}
    }
    if (tq != &VLRU) lruvcp = QTOV(tq);
    else lruvcp = QTOV(VLRU.next);

    /* now we have to hold this entry, so that it does not get moved
     * into the free list while we're running.  It could still get
     * moved within the lru queue, but hopefully that will be rare; it
     * doesn't hurt nearly as much.
     */
    retry = 0;
    osi_vnhold(lruvcp, &retry);
    ReleaseReadLock(&afs_xvcache);           /* could be read lock */
    if (retry)
      goto reskip;

    /* otherwise, merge in the info.  We have to be quite careful here,
     * since we need to ensure that we don't merge old info over newer
     * stuff in a stat cache entry.  We're very conservative here: we don't
     * do the merge at all unless we ourselves create the stat cache
     * entry.  That's pretty safe, and should work pretty well, since we
     * typically expect to do the stat cache creation ourselves.
     *
     * We also have to take into account racing token revocations.
     */
    for(i=0; i<fidIndex; i++) {
	afid.Cell = adp->fid.Cell;
	afid.Fid.Volume = adp->fid.Fid.Volume;
	afid.Fid.Vnode = fidsp[i].Vnode;
	afid.Fid.Unique = fidsp[i].Unique;
	do {
	   retry = 0;
	   ObtainReadLock(&afs_xvcache);
	   tvcp = afs_FindVCache(&afid, 1, 0, &retry, 0/* !stats&!lru*/);
	   ReleaseReadLock(&afs_xvcache);
	} while (tvcp && retry);

	/* The entry may no longer exist */
	if (tvcp == NULL) {
	    continue;
	}

	/* now we have the entry held, but we need to fill it in */
	ObtainWriteLock(&tvcp->lock,131);

	/* if CBulkFetching is not set, or if the file size no longer
	 * matches the value we placed there when we set the CBulkFetching
	 * flag, then someone else has done something with this node,
	 * and we may not have the latest status information for this
	 * file.  Leave the entry alone.
	 */
	if (!(tvcp->states & CBulkFetching) || (tvcp->m.Length != statSeqNo)) {
	    flagIndex++;
	    ReleaseWriteLock(&tvcp->lock);
	    afs_PutVCache(tvcp);
	    continue;
	}

	/* now copy ".." entry back out of volume structure, if necessary */
	if (tvcp->mvstat == 2  && (dotdot.Fid.Volume != 0)) {
	    if (!tvcp->mvid)
		tvcp->mvid = (struct VenusFid *) osi_AllocSmallSpace(sizeof(struct VenusFid));
	    *tvcp->mvid = dotdot;
	}

	ObtainWriteLock(&afs_xvcache,132);
	if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
	   refpanic ("Bulkstat VLRU inconsistent2");
	}
	if ((QNext(QPrev(&tvcp->vlruq)) != &tvcp->vlruq) 
	    || (QPrev(QNext(&tvcp->vlruq)) != &tvcp->vlruq))
	   refpanic ("Bulkstat VLRU inconsistent4");
	if ((QNext(QPrev(&lruvcp->vlruq)) != &lruvcp->vlruq) 
	    || (QPrev(QNext(&lruvcp->vlruq)) != &lruvcp->vlruq)) 
	   refpanic ("Bulkstat VLRU inconsistent5");

	if (tvcp != lruvcp) {  /* if they are == don't move it, don't corrupt vlru */
	   QRemove(&tvcp->vlruq);
	   QAdd(&lruvcp->vlruq, &tvcp->vlruq);
	}

	if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
	   refpanic ("Bulkstat VLRU inconsistent3");
	}
	if ((QNext(QPrev(&tvcp->vlruq)) != &tvcp->vlruq) 
	    || (QPrev(QNext(&tvcp->vlruq)) != &tvcp->vlruq))
	   refpanic ("Bulkstat VLRU inconsistent5");
	if ((QNext(QPrev(&lruvcp->vlruq)) != &lruvcp->vlruq) 
	    || (QPrev(QNext(&lruvcp->vlruq)) != &lruvcp->vlruq))
	   refpanic ("Bulkstat VLRU inconsistent6");
	ReleaseWriteLock(&afs_xvcache);

	/* We need to check the flags again. We may have missed
	 * something while we were waiting for a lock.
	 */
	if (!(tvcp->states & CBulkFetching) || (tvcp->m.Length != statSeqNo)) {
	    flagIndex++;
	    ReleaseWriteLock(&tvcp->lock);
	    afs_PutVCache(tvcp);
	    continue;
	}

	/* now merge in the resulting status back into the vnode.
	 * We only do this if the entry looks clear.
	 */
	afs_ProcessFS(tvcp, &statsp[i], areqp);
#ifdef AFS_LINUX22_ENV
	/* overwrite the ops if it's a directory or symlink. */
	if (vType(tvcp) == VDIR)
	    tvcp->v.v_op = &afs_dir_iops;
	else if (vType(tvcp) == VLNK)
	    tvcp->v.v_op = &afs_symlink_iops;
#endif

	ObtainWriteLock(&afs_xcbhash, 494);

	/* We need to check the flags once more. We may have missed
	 * something while we were waiting for a lock.
	 */
	if (!(tvcp->states & CBulkFetching) || (tvcp->m.Length != statSeqNo)) {
	    flagIndex++;
	    ReleaseWriteLock(&afs_xcbhash);
	    ReleaseWriteLock(&tvcp->lock);
	    afs_PutVCache(tvcp);
	    continue;
	}

	/* do some accounting for bulk stats: mark this entry as
	 * loaded, so we can tell if we use it before it gets
	 * recycled.
	 */
	tvcp->states |= CBulkStat;
	tvcp->states &= ~CBulkFetching;
	flagIndex++;
	afs_bulkStatsDone++;

	/* merge in vol info */
	if (volStates & VRO) tvcp->states |= CRO;
	if (volStates & VBackup) tvcp->states |= CBackup;
	if (volStates & VForeign) tvcp->states |= CForeign;

	/* merge in the callback info */
	tvcp->states |= CTruth;

	/* get ptr to the callback we are interested in */
	tcbp = cbsp + i;

	if (tcbp->ExpirationTime != 0) {
	    tvcp->cbExpires = tcbp->ExpirationTime+startTime;
	    tvcp->callback = hostp;
	    tvcp->states |= CStatd;
	    afs_QueueCallback(tvcp, CBHash(tcbp->ExpirationTime), volp);
	}
	else if (tvcp->states & CRO) {
	    /* ordinary callback on a read-only volume -- AFS 3.2 style */
	    tvcp->cbExpires = 3600+startTime;
	    tvcp->callback = hostp;
	    tvcp->states |= CStatd;
	    afs_QueueCallback(tvcp, CBHash(3600), volp);
	}
	else {
	    tvcp->callback = 0;
	    tvcp->states &= ~(CStatd|CUnique);  
	    afs_DequeueCallback(tvcp);
	    if ((tvcp->states & CForeign) || (vType(tvcp) == VDIR)) 
	      osi_dnlc_purgedp (tvcp);  /* if it (could be) a directory */
	}
	ReleaseWriteLock(&afs_xcbhash);

	ReleaseWriteLock(&tvcp->lock);
	/* finally, we're done with the entry */
	afs_PutVCache(tvcp);
    }	/* for all files we got back */

    /* finally return the pointer into the LRU queue */
    afs_PutVCache(lruvcp);

  done:
    /* Be sure to turn off the CBulkFetching flags */
    for(i=flagIndex; i<fidIndex; i++) {
	afid.Cell = adp->fid.Cell;
	afid.Fid.Volume = adp->fid.Fid.Volume;
	afid.Fid.Vnode = fidsp[i].Vnode;
	afid.Fid.Unique = fidsp[i].Unique;
	do {
	   retry = 0;
	   ObtainReadLock(&afs_xvcache);
	   tvcp = afs_FindVCache(&afid, 1, 0, &retry, 0/* !stats&!lru*/);
	   ReleaseReadLock(&afs_xvcache);
	} while (tvcp && retry);
	if (tvcp != NULL
	    && (tvcp->states & CBulkFetching)
	    && (tvcp->m.Length == statSeqNo)) {
	  tvcp->states &= ~CBulkFetching;
	}
	if (tvcp != NULL) {
	  afs_PutVCache(tvcp);
	}
    }
    if ( volp )
	afs_PutVolume(volp, READ_LOCK);
    
    osi_FreeLargeSpace(statMemp);
    osi_FreeLargeSpace(cbfMemp);
}

/* was: (AFS_DEC_ENV) || defined(AFS_OSF30_ENV) || defined(AFS_NCR_ENV) */
int AFSDOBULK = 1;

#ifdef	AFS_OSF_ENV
afs_lookup(adp, ndp)
    struct vcache *adp;
    struct nameidata *ndp; {
    char aname[MAXNAMLEN+1];	/* XXX */
    struct vcache **avcp = (struct vcache **)&(ndp->ni_vp);
    struct ucred *acred = ndp->ni_cred;
    int wantparent = ndp->ni_nameiop & WANTPARENT;
    int opflag = ndp->ni_nameiop & OPFLAG;
#else	/* AFS_OSF_ENV */
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
afs_lookup(OSI_VC_ARG(adp), aname, avcp, pnp, flags, rdir, acred)
    struct pathname *pnp;
    int flags;
    struct vnode *rdir;
#else
afs_lookup(adp, aname, avcp, acred)
#endif
    OSI_VC_DECL(adp);
    struct vcache **avcp;
    char *aname;
    struct AFS_UCRED *acred; {
#endif
    struct vrequest treq;
    char *tname = (char *)0;
    register struct vcache *tvc=0;
    register afs_int32 code;
    int pass = 0, hit = 0;
    long dirCookie;
    extern afs_int32 afs_mariner;			/*Writing activity to log?*/
    OSI_VC_CONVERT(adp)
    afs_hyper_t versionNo;

    AFS_STATCNT(afs_lookup);
#ifdef	AFS_OSF_ENV
    ndp->ni_dvp = (struct vnode *)adp;
    bcopy(ndp->ni_ptr, aname, ndp->ni_namelen);
    aname[ndp->ni_namelen] = '\0';
#endif	/* AFS_OSF_ENV */

    *avcp = (struct vcache *) 0;   /* Since some callers don't initialize it */

    if (code = afs_InitReq(&treq, acred)) { 
      goto done;
    }

    /* lookup the name aname in the appropriate dir, and return a cache entry
      on the resulting fid */

    /*
     * check for, and handle "@sys" if it's there.  We should be able
     * to avoid the alloc and the strcpy with a little work, but it's
     * not pressing.  If there aren't any remote users (ie, via the 
     * NFS translator), we have a slightly easier job.
     * the faster way to do this is to check for *aname == '@' and if 
     * it's there, check for @sys, otherwise, assume there's no @sys 
     * then, if the lookup fails, check for .*@sys...
     */
    if (!AFS_EQ_ATSYS(aname)) {
      tname = aname;
    }
    else {
	tname = (char *) osi_AllocLargeSpace(AFS_SMALLOCSIZ);
	if (!afs_nfsexporter) 
	  strcpy(tname, (afs_sysname ? afs_sysname : SYS_NAME ));
	else {
	  register struct unixuser *au;
	  register afs_int32 error;
	  au = afs_GetUser(treq.uid, adp->fid.Cell, 0); afs_PutUser(au, 0);	
	  if (au->exporter) {
	    error = EXP_SYSNAME(au->exporter, (char *)0, tname);
	    if (error) 
	      strcpy(tname, "@sys");
	  } else {
	      strcpy(tname, (afs_sysname ? afs_sysname : SYS_NAME ));
	  }
	}
      }

    /* come back to here if we encounter a non-existent object in a read-only
       volume's directory */

  redo:
    *avcp = (struct vcache *) 0;   /* Since some callers don't initialize it */

    if (!(adp->states & CStatd)) {
	if (code = afs_VerifyVCache2(adp, &treq))
	  goto done;
    }
    else code = 0;

    /* watch for ".." in a volume root */
    if (adp->mvstat == 2 && tname[0] == '.' && tname[1] == '.' && !tname[2]) {
	/* looking up ".." in root via special hacks */
	if (adp->mvid == (struct VenusFid *) 0 || adp->mvid->Fid.Volume == 0) {
#ifdef	AFS_OSF_ENV
	    extern struct vcache *afs_globalVp;
	    if (adp == afs_globalVp) {
		struct vnode *rvp = (struct vnode *)adp;
/*
		ndp->ni_vp = rvp->v_vfsp->vfs_vnodecovered;
		ndp->ni_dvp = ndp->ni_vp;
		VN_HOLD(*avcp);
*/
		code = ENODEV;
		goto done;
	    }
#endif
	    code = ENODEV;
	    goto done;
	}
	/* otherwise we have the fid here, so we use it */
	tvc = afs_GetVCache(adp->mvid, &treq, (afs_int32 *)0,
			    (struct vcache*)0, 0);
	afs_Trace3(afs_iclSetp, CM_TRACE_GETVCDOTDOT,
		   ICL_TYPE_FID, adp->mvid, ICL_TYPE_POINTER, tvc, 
		   ICL_TYPE_INT32,  code);
	*avcp = tvc;
	code = (tvc ? 0 : ENOENT);
	hit = 1;
	if (tvc && !tvc->vrefCount) {
	    osi_Panic("TT1");
	}
	if (code) {
	    /*printf("LOOKUP GETVCDOTDOT -> %d\n", code);*/
	}
	goto done;
    }

    /* now check the access */
    if (treq.uid != adp->last_looker) {  
       if (!afs_AccessOK(adp, PRSFS_LOOKUP, &treq, CHECK_MODE_BITS)) {
	 *avcp = (struct vcache *)0;
	 code = EACCES;
	 goto done;
       }
       else adp->last_looker = treq.uid;
    } 


    /* special case lookup of ".".  Can we check for it sooner in this code,
     * for instance, way up before "redo:" ??
     * I'm not fiddling with the LRUQ here, either, perhaps I should, or else 
     * invent a lightweight version of GetVCache.
     */
    if (tname[0] == '.' && !tname[1]) { /* special case */
	ObtainReadLock(&afs_xvcache);	
	osi_vnhold(adp, 0);
	ReleaseReadLock(&afs_xvcache);	
      code = 0;
      *avcp = tvc = adp;
      hit = 1;
	if (adp && !adp->vrefCount) {
	    osi_Panic("TT2");
	}
      goto done;
    }

    tvc = osi_dnlc_lookup (adp, tname, WRITE_LOCK);
    *avcp = tvc;  /* maybe wasn't initialized, but it is now */
#ifdef AFS_LINUX22_ENV
    if (tvc) {
      if (tvc->mvstat == 2) { /* we don't trust the dnlc for root vcaches */
	AFS_RELE(tvc);
	*avcp = 0;
      }
      else {  
	code = 0;
	hit = 1;
	goto done;
      }
    }
#else /* non - LINUX */
    if (tvc) {
      code = 0;
      hit = 1;
      goto done;
    }
#endif /* linux22 */

    {
    register struct dcache *tdc;
    afs_int32 dirOffset, dirLen;
    ino_t theDir;
    struct VenusFid tfid;

    /* now we have to lookup the next fid */
    tdc = afs_GetDCache(adp, 0, &treq, &dirOffset, &dirLen, 1);
    if (!tdc) {
      *avcp = (struct vcache *)0;  /* redundant, but harmless */
      code = EIO;
      goto done;
    }

    /* now we will just call dir package with appropriate inode.
      Dirs are always fetched in their entirety for now */
    /* If the first lookup doesn't succeed, maybe it's got @sys in the name */
    ObtainReadLock(&adp->lock);

    /*
     * Make sure that the data in the cache is current. There are two
     * cases we need to worry about:
     * 1. The cache data is being fetched by another process.
     * 2. The cache data is no longer valid
     */
    while ((adp->states & CStatd)
	   && (tdc->flags & DFFetching)
	   && hsame(adp->m.DataVersion, tdc->f.versionNo)) {
	tdc->flags |= DFWaiting;
	ReleaseReadLock(&adp->lock);
	afs_osi_Sleep(&tdc->validPos);
	ObtainReadLock(&adp->lock);
    }
    if (!(adp->states & CStatd)
	|| !hsame(adp->m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&adp->lock);
	afs_PutDCache(tdc);
	goto redo;
    }

    /* Save the version number for when we call osi_dnlc_enter */
    hset(versionNo, tdc->f.versionNo);

    theDir = tdc->f.inode;
    code = afs_dir_LookupOffset(&theDir, tname, &tfid.Fid, &dirCookie);
    if (code == ENOENT && tname == aname) {
      int len;
      len = strlen(aname);
      if (len >= 4 && AFS_EQ_ATSYS(aname+len-4)) {
	tname = (char *) osi_AllocLargeSpace(AFS_LRALLOCSIZ);
	afs_HandleAtName(aname, tname, &treq, adp);
	code = afs_dir_LookupOffset(&theDir, tname, &tfid.Fid, &dirCookie);
      }
    }
    ReleaseReadLock(&adp->lock);
    afs_PutDCache(tdc);

    /* new fid has same cell and volume */
    tfid.Cell = adp->fid.Cell;
    tfid.Fid.Volume = adp->fid.Fid.Volume;
    afs_Trace4(afs_iclSetp, CM_TRACE_LOOKUP, ICL_TYPE_POINTER, adp, 
	       ICL_TYPE_STRING, tname,
	       ICL_TYPE_FID, &tfid, ICL_TYPE_INT32, code);

    if (code) {
	if (code != ENOENT) {
	    printf("LOOKUP dirLookupOff -> %d\n", code);
	}
	goto done;
    }  

    /* prefetch some entries, if the dir is currently open.  The variable
     * dirCookie tells us where to start prefetching from.
     */
    if (AFSDOBULK && adp->opens > 0 && !(adp->states & CForeign)) {
        afs_int32 retry;
	/* if the entry is not in the cache, or is in the cache,
	 * but hasn't been statd, then do a bulk stat operation.
	 */
        do {
	   retry = 0;
	   ObtainReadLock(&afs_xvcache);	
	   tvc = afs_FindVCache(&tfid, 1, 0, &retry, 0/* !stats,!lru */);
	   ReleaseReadLock(&afs_xvcache);	
        } while (tvc && retry);

	if (!tvc || !(tvc->states & CStatd)) {
	    afs_DoBulkStat(adp, dirCookie, &treq);
	}

	/* if the vcache isn't usable, release it */
	if (tvc && !(tvc->states & CStatd)) {
	    afs_PutVCache(tvc);
	    tvc = (struct vcache *) 0;
	}
    }
    else tvc = (struct vcache *) 0;
    
    /* now get the status info, if we don't already have it */
    /* This is kind of weird, but we might wind up accidentally calling
     * RXAFS_Lookup because we happened upon a file which legitimately
     * has a 0 uniquifier. That is the result of allowing unique to wrap
     * to 0. This was fixed in AFS 3.4. For CForeigh, Unique == 0 means that
     * the file has not yet been looked up.
     */
    if (!tvc) {
       afs_int32 cached = 0;
       if (!tfid.Fid.Unique && (adp->states & CForeign)) {
	    tvc = afs_LookupVCache(&tfid, &treq, &cached, WRITE_LOCK, 
				   adp, tname);
       } 
       if (!tvc) {  /* lookup failed or wasn't called */
	    tvc = afs_GetVCache(&tfid, &treq, &cached, (struct vcache*)0,
				WRITE_LOCK);
       }
    } /* if !tvc */
    } /* sub-block just to reduce stack usage */

    if (tvc) {
       if (adp->states & CForeign)
	   tvc->states |= CForeign;
	tvc->parentVnode = adp->fid.Fid.Vnode;
	tvc->parentUnique = adp->fid.Fid.Unique;
	tvc->states &= ~CBulkStat;
	if (tvc->mvstat == 1) {
	  /* a mt point, possibly unevaluated */
	  struct volume *tvolp;

	    ObtainWriteLock(&tvc->lock,133);
	    code = EvalMountPoint(tvc, adp, &tvolp, &treq);
	    ReleaseWriteLock(&tvc->lock);
	    /* next, we want to continue using the target of the mt point */
	    if (tvc->mvid && (tvc->states & CMValid)) {
	      struct vcache *uvc;
		/* now lookup target, to set .. pointer */
		afs_Trace2(afs_iclSetp, CM_TRACE_LOOKUP1,
			   ICL_TYPE_POINTER, tvc, ICL_TYPE_FID, &tvc->fid);
		uvc = tvc;	/* remember for later */

		if (tvolp && (tvolp->states & VForeign)) {
		    /* XXXX tvolp has ref cnt on but not locked! XXX */
		    tvc = afs_GetRootVCache(tvc->mvid, &treq, (afs_int32 *)0, tvolp, WRITE_LOCK);
		} else {
		    tvc = afs_GetVCache(tvc->mvid, &treq, (afs_int32 *)0,
					(struct vcache*)0, WRITE_LOCK);
		}
		afs_PutVCache(uvc, WRITE_LOCK); /* we're done with it */

		if (!tvc) {
		    code = ENOENT;
		    if (tvolp) {
			afs_PutVolume(tvolp, WRITE_LOCK);
		    }
		    goto done;
		}

		/* now, if we came via a new mt pt (say because of a new
		 * release of a R/O volume), we must reevaluate the ..
		 * ptr to point back to the appropriate place */
		if (tvolp) {
		    ObtainWriteLock(&tvc->lock,134);
		    if (tvc->mvid == (struct VenusFid *) 0) {
			tvc->mvid = (struct VenusFid *) osi_AllocSmallSpace(sizeof(struct VenusFid));
		    }
		    /* setup backpointer */
		    *tvc->mvid = tvolp->dotdot;
		    ReleaseWriteLock(&tvc->lock);
		    afs_PutVolume(tvolp, WRITE_LOCK);
		}
	    }
	    else {
		afs_PutVCache(tvc, WRITE_LOCK);
		code = ENOENT;
		if (tvolp) afs_PutVolume(tvolp, WRITE_LOCK);
		goto done;
	    }
	}
	*avcp = tvc;
	if (tvc && !tvc->vrefCount) {
	    osi_Panic("TT3");
	}
	code = 0;
    }
    else {
	/* if we get here, we found something in a directory that couldn't
	   be located (a Multics "connection failure").  If the volume is
	   read-only, we try flushing this entry from the cache and trying
	   again. */
	if (pass == 0) {
	    struct volume *tv;
	    tv = afs_GetVolume(&adp->fid, &treq, READ_LOCK);
	    if (tv) {
		if (tv->states & VRO) {
		    pass = 1;			/* try this *once* */
		    ObtainWriteLock(&afs_xcbhash, 495);
		    afs_DequeueCallback(adp);
		    /* re-stat to get later version */
		    adp->states &= ~CStatd;
		    ReleaseWriteLock(&afs_xcbhash);
		    osi_dnlc_purgedp(adp);
		    afs_PutVolume(tv, READ_LOCK);
		    goto redo;
		}
		afs_PutVolume(tv, READ_LOCK);
	    }
	}
	code = ENOENT;
    }

done:
    /* put the network buffer back, if need be */
    if (tname != aname && tname) osi_FreeLargeSpace(tname);
    if (code == 0) {
#ifdef	AFS_OSF_ENV
	/* Handle RENAME; only need to check rename "."  */
	if (opflag == RENAME && wantparent && *ndp->ni_next == 0) {
	    if (!FidCmp(&(tvc->fid), &(adp->fid))) { 
		afs_PutVCache(*avcp, WRITE_LOCK);
		*avcp = NULL;
		return afs_CheckCode(EISDIR, &treq, 18);
	    }
	}
#endif	/* AFS_OSF_ENV */

	if (afs_mariner)
	  afs_AddMarinerName(aname, tvc); 
	if (!hit) {
	  osi_dnlc_enter (adp, aname, tvc, &versionNo);
	}
	else {
#ifdef AFS_LINUX20_ENV
	    /* So Linux inode cache is up to date. */
	    code = afs_VerifyVCache(tvc, &treq);
#else
	    return 0;  /* can't have been any errors if hit and !code */
#endif
	}
    }
    code = afs_CheckCode(code, &treq, 19);
    if (code) {
       /* If there is an error, make sure *avcp is null.
	* Alphas panic otherwise - defect 10719.
	*/
       *avcp = (struct vcache *)0;
    }

    return code;
}
