/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_vnop_dirops.c - make and remove directories
 *
 * Implements:
 *
 * afs_mkdir
 * afs_rmdir
 *
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"

extern afs_rwlock_t afs_xvcache;
extern afs_rwlock_t afs_xcbhash;

/* don't set CDirty in here because RPC is called synchronously */

int
afs_mkdir(OSI_VC_DECL(adp), char *aname, struct vattr *attrs, 
     register struct vcache **avcp, struct AFS_UCRED *acred)
{
    struct vrequest treq;
    register afs_int32 code;
    register struct conn *tc;
    struct VenusFid newFid;
    register struct dcache *tdc;
    afs_size_t offset, len;
    register struct vcache *tvc;
    struct AFSStoreStatus InStatus;
    struct AFSFetchStatus OutFidStatus, OutDirStatus;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    afs_int32 now;
    struct afs_fakestat_state fakestate;
    XSTATS_DECLS;
    OSI_VC_CONVERT(adp);

    AFS_STATCNT(afs_mkdir);
    afs_Trace2(afs_iclSetp, CM_TRACE_MKDIR, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_STRING, aname);

    if ((code = afs_InitReq(&treq, acred)))
	goto done2;
    afs_InitFakeStat(&fakestate);

    if (strlen(aname) > AFSNAMEMAX) {
	code = ENAMETOOLONG;
	goto done;
    }

    if (!afs_ENameOK(aname)) {
	code = EINVAL;
	goto done;
    }
    code = afs_EvalFakeStat(&adp, &fakestate, &treq);
    if (code)
	goto done;
    code = afs_VerifyVCache(adp, &treq);
    if (code)
	goto done;

    /** If the volume is read-only, return error without making an RPC to the
      * fileserver
      */
    if (adp->states & CRO) {
	code = EROFS;
	goto done;
    }

    InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE | AFS_SETGROUP;
    InStatus.ClientModTime = osi_Time();
    InStatus.UnixModeBits = attrs->va_mode & 0xffff;	/* only care about protection bits */
    InStatus.Group = (afs_int32) acred->cr_gid;
    tdc = afs_GetDCache(adp, (afs_size_t) 0, &treq, &offset, &len, 1);
#ifdef DISCONN
    /* do some error checking to make sure this doesn't already exist */

    if(!tdc) {
        uprintf("Failed to make directory, can't get parent \n");
        return ENETDOWN;
        }

    code = dir_Lookup(&tdc->f.inode, aname, &newFid.Fid);
    if (!code) {
        afs_PutDCache(tdc);
        return ENOENT;
    }


    if (LOG_OPERATIONS(discon_state)) {
        struct dcache *newdc;
        long cur_op;
        /*
	**  We need to get a new fid, then get a dcache and a vcache
        **  entry, and store the items in our local cache
        */

        generateDFID(adp,&newFid);


        ObtainWriteLock(&afs_xvcache);
        tvc = afs_NewVCache(&newFid, 0, 0, 0);

        if(tvc->index==NULLIDX) panic(" afs_mkdir: got tvc with bad index");

        ObtainWriteLock(&adp->lock);
        ObtainWriteLock(&tvc->lock);
        ReleaseWriteLock(&afs_xvcache);

        *avcp = tvc;
        code = dir_Create(&tdc->f.inode, aname, &newFid.Fid);


        tvc->m.Owner = osi_curcred()->cr_uid;
        tvc->m.Group = osi_curcred()->cr_gid;

        hzero(tvc->m.DataVersion);

        /*  XXX Nasty hack  for len, fix! */
        tvc->m.Length =  2048;
        tvc->m.LinkCount =  2;
        tvc->m.Date = osi_Time();
        tvc->m.Mode = attrs->va_mode & 0xffff;

        tvc->callback = 0;
        /* clear callback flag */
        afs_VindexFlags[tvc->index] &= ~VC_HAVECB;
        tvc->cbExpires = osi_Time();
        tvc->states |= CStatd;
        vSetType(tvc,VDIR);
        tvc->m.Mode |= S_IFDIR;



        /* XXX create the data for it! */
        newdc = get_newDCache(tvc);
        if(!newdc) panic("failed to get a newDC ");
        code = dir_MakeDir(&newdc->f.inode,&tvc->fid.Fid,&adp->fid.Fid);

        tvc->parentVnode = adp->fid.Fid.Vnode;
        tvc->parentUnique = adp->fid.Fid.Unique;

        cur_op = log_dis_mkdir(tvc, adp, attrs, aname);

        /* mark the vcaches to lock them into the cache */
        adp->last_mod_op = cur_op;
        adp->dflags |= (KEEP_VC | VC_DIRTY);
        afs_VindexFlags[adp->index] |= KEEP_VC;
        ReleaseWriteLock(&adp->lock);

        tvc->last_mod_op = cur_op;
        tvc->dflags |= (KEEP_VC | VC_DIRTY);
        afs_VindexFlags[tvc->index] |= KEEP_VC;


        /* mark the dcaches that need to be kept in the cache */
        tdc->f.dflags |= KEEP_DC;
        afs_indexFlags[tdc->index] |= IFFKeep_DC;
        tdc->flags |= DFEntryMod;
        newdc->f.dflags |= KEEP_DC;
        afs_indexFlags[newdc->index] |= IFFKeep_DC;
        newdc->flags |= DFEntryMod;

        ReleaseWriteLock(&tvc->lock);

        return 0;
    }
#endif  /* DISCONN */
    ObtainWriteLock(&adp->lock, 153);
    do {
	tc = afs_Conn(&adp->fid, &treq, SHARED_LOCK);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_MAKEDIR);
	    now = osi_Time();
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_MakeDir(tc->id, (struct AFSFid *)&adp->fid.Fid, aname,
			      &InStatus, (struct AFSFid *)&newFid.Fid,
			      &OutFidStatus, &OutDirStatus, &CallBack,
			      &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	    CallBack.ExpirationTime += now;
	    /* DON'T forget to Set the callback value... */
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, code, &adp->fid, &treq, AFS_STATS_FS_RPCIDX_MAKEDIR,
	      SHARED_LOCK, NULL));

    if (code) {
	if (code < 0) {
	    ObtainWriteLock(&afs_xcbhash, 490);
	    afs_DequeueCallback(adp);
	    adp->states &= ~CStatd;
	    ReleaseWriteLock(&afs_xcbhash);
	    osi_dnlc_purgedp(adp);
	}
	ReleaseWriteLock(&adp->lock);
	if (tdc)
	    afs_PutDCache(tdc);
	goto done;
    }
    /* otherwise, we should see if we can make the change to the dir locally */
    if (tdc)
	ObtainWriteLock(&tdc->lock, 632);
    if (afs_LocalHero(adp, tdc, &OutDirStatus, 1)) {
	/* we can do it locally */
	code = afs_dir_Create(tdc, aname, &newFid.Fid);
	if (code) {
	    ZapDCE(tdc);	/* surprise error -- use invalid value */
	    DZap(tdc);
	}
    }
    if (tdc) {
	ReleaseWriteLock(&tdc->lock);
	afs_PutDCache(tdc);
    }
    adp->m.LinkCount = OutDirStatus.LinkCount;
    newFid.Cell = adp->fid.Cell;
    newFid.Fid.Volume = adp->fid.Fid.Volume;
    ReleaseWriteLock(&adp->lock);
    /* now we're done with parent dir, create the real dir's cache entry */
    tvc = afs_GetVCache(&newFid, &treq, NULL, NULL);
    if (tvc) {
	code = 0;
	*avcp = tvc;
    } else
	code = ENOENT;
  done:
    afs_PutFakeStat(&fakestate);
    code = afs_CheckCode(code, &treq, 26);
  done2:
    return code;
}


int
/* don't set CDirty in here because RPC is called synchronously */
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
afs_rmdir(OSI_VC_DECL(adp), char *aname, struct vnode *cdirp, 
	  struct AFS_UCRED *acred)
#else
afs_rmdir(OSI_VC_DECL(adp), char *aname, struct AFS_UCRED *acred)
#endif
{
    struct vrequest treq;
    register struct dcache *tdc;
    register struct vcache *tvc = NULL;
    register afs_int32 code;
    register struct conn *tc;
    afs_size_t offset, len;
    struct AFSFetchStatus OutDirStatus;
    struct AFSVolSync tsync;
    struct afs_fakestat_state fakestate;
    XSTATS_DECLS;
    OSI_VC_CONVERT(adp);

    AFS_STATCNT(afs_rmdir);

    afs_Trace2(afs_iclSetp, CM_TRACE_RMDIR, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_STRING, aname);

    if ((code = afs_InitReq(&treq, acred)))
	goto done2;
    afs_InitFakeStat(&fakestate);

    if (strlen(aname) > AFSNAMEMAX) {
	code = ENAMETOOLONG;
	goto done;
    }

    code = afs_EvalFakeStat(&adp, &fakestate, &treq);
    if (code)
	goto done;

    code = afs_VerifyVCache(adp, &treq);
    if (code)
	goto done;

    /** If the volume is read-only, return error without making an RPC to the
      * fileserver
      */
    if (adp->states & CRO) {
	code = EROFS;
	goto done;
    }

    tdc = afs_GetDCache(adp, (afs_size_t) 0, &treq, &offset, &len, 1);	/* test for error below */
ifdef  DISCONN
  /* Check to make sure a dcache was returned */
    if(!tdc) return ENETDOWN;
#endif  /* DISCONN */
    ObtainWriteLock(&adp->lock, 154);
    if (tdc)
	ObtainSharedLock(&tdc->lock, 633);
#ifdef  DISCONN
    /*
    **  This is the stuff we do when we are running is disconnected mode.
    **  First we need to check all the conditions to make sure it is
    **  legal for us to remove the directory.  If it is not, then
    **  we need to return an error message.
    */
    if (LOG_OPERATIONS(discon_state)) {
        struct vcache *dvc;
        struct dcache *ddc;
        struct VenusFid dirFid;
        long cur_op;
        int     namelen = strlen(aname);


        /*  we need to get the dcache entries of the directory */
        code = dir_Lookup(&tdc->f.inode, aname, &dirFid.Fid);
        dirFid.Cell = adp->fid.Cell;
        dirFid.Fid.Volume = adp->fid.Fid.Volume;


        ObtainWriteLock(&afs_xvcache);
        dvc = afs_FindVCache(&dirFid, 0, 0);
        ReleaseWriteLock(&afs_xvcache);
        ddc = afs_GetDCache(dvc, 0, &treq, &offset, &len, 1);
        /* Check some error conditions */
        if(!ddc)
                code = ENETDOWN;

        if ((!code) && (namelen == 1 && bcmp(aname, ".",1) ==0))
                code = EINVAL;

        if((!code) && (namelen == 2 && bcmp(aname, "..",2) ==0))
                code = ENOTEMPTY;

        /* Let's make sure there are no entries in the directory */
        if (!code)
                code = dir_IsEmpty(&ddc->f.inode);

        if (code) {
                ReleaseWriteLock(&adp->lock);
                afs_PutVCache(dvc, 0);
                if (tdc) {
		    ReleaseSharedLock(&tdc->lock);
		    afs_PutDCache(tdc);
		}
                afs_PutDCache(ddc);
                return (code);
        }


        /* everything looks good, log the rmdir */
        ObtainWriteLock(&dvc->lock);

        cur_op = log_dis_rmdir(dvc, adp, aname);

        /* mark the vcaches to lock them into the cache */
        dvc->last_mod_op = cur_op;
        dvc->dflags |= (KEEP_VC | VC_DIRTY);
        afs_VindexFlags[dvc->index] |= KEEP_VC;
        ReleaseWriteLock(&dvc->lock);

        adp->last_mod_op = cur_op;
        adp->dflags |= (KEEP_VC | VC_DIRTY);
        afs_VindexFlags[adp->index] |= KEEP_VC;

        /* mark the dcaches that need to be kept in the cache */
        ddc->f.dflags |= KEEP_DC;
        afs_indexFlags[ddc->index] |= IFFKeep_DC;
        ddc->flags |= DFEntryMod;

        tdc->f.dflags |= KEEP_DC;
        afs_indexFlags[tdc->index] |= IFFKeep_DC;
        tdc->flags |= DFEntryMod;

        /* do some house-cleaning on our local cache */
        code = dir_Delete(&tdc->f.inode, aname);

        ReleaseWriteLock(&adp->lock);
        afs_PutDCache(tdc);     /* drop ref count */
        afs_PutDCache(ddc);     /* drop ref count */
        afs_PutVCache(dvc, 0);     /* drop ref count */
        return 0;

    }
    else

#endif  /* DISCONN */
    if (tdc && (adp->states & CForeign)) {
	struct VenusFid unlinkFid;

	unlinkFid.Fid.Vnode = 0;
	code = afs_dir_Lookup(tdc, aname, &unlinkFid.Fid);
	if (code == 0) {
	    afs_int32 cached = 0;

	    unlinkFid.Cell = adp->fid.Cell;
	    unlinkFid.Fid.Volume = adp->fid.Fid.Volume;
	    if (unlinkFid.Fid.Unique == 0) {
		tvc =
		    afs_LookupVCache(&unlinkFid, &treq, &cached, adp, aname);
	    } else {
		ObtainReadLock(&afs_xvcache);
		tvc = afs_FindVCache(&unlinkFid, 0, 1 /* do xstats */ );
		ReleaseReadLock(&afs_xvcache);
	    }
	}
    }

    do {
	tc = afs_Conn(&adp->fid, &treq, SHARED_LOCK);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEDIR);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_RemoveDir(tc->id, (struct AFSFid *)&adp->fid.Fid, aname,
				&OutDirStatus, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, code, &adp->fid, &treq, AFS_STATS_FS_RPCIDX_REMOVEDIR,
	      SHARED_LOCK, NULL));

    if (code) {
	if (tdc) {
	    ReleaseSharedLock(&tdc->lock);
	    afs_PutDCache(tdc);
	}
	if (code < 0) {
	    ObtainWriteLock(&afs_xcbhash, 491);
	    afs_DequeueCallback(adp);
	    adp->states &= ~CStatd;
	    ReleaseWriteLock(&afs_xcbhash);
	    osi_dnlc_purgedp(adp);
	}
	ReleaseWriteLock(&adp->lock);
	goto done;
    }
    /* here if rpc worked; update the in-core link count */
    adp->m.LinkCount = OutDirStatus.LinkCount;
    if (tdc)
	UpgradeSToWLock(&tdc->lock, 634);
    if (afs_LocalHero(adp, tdc, &OutDirStatus, 1)) {
	/* we can do it locally */
	code = afs_dir_Delete(tdc, aname);
	if (code) {
	    ZapDCE(tdc);	/* surprise error -- invalid value */
	    DZap(tdc);
	}
    }
    if (tdc) {
	ReleaseWriteLock(&tdc->lock);
	afs_PutDCache(tdc);	/* drop ref count */
    }


    if (tvc) {
	osi_dnlc_purgedp(tvc);	/* get rid of any entries for this directory */
	afs_symhint_inval(tvc);
    } else
	osi_dnlc_remove(adp, aname, 0);

    if (tvc) {
	ObtainWriteLock(&tvc->lock, 155);
	tvc->states &= ~CUnique;	/* For the dfs xlator */
	ReleaseWriteLock(&tvc->lock);
	afs_PutVCache(tvc);
    }
    ReleaseWriteLock(&adp->lock);
    /* don't worry about link count since dirs can not be hardlinked */
    code = 0;

  done:
    afs_PutFakeStat(&fakestate);
    code = afs_CheckCode(code, &treq, 27);
  done2:
    return code;
}
