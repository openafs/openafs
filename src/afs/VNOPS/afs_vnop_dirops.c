/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * For copyright information, see IPL which you accepted in order to
 * download this software.
 *
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

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h" /* statistics */
#include "../afs/afs_cbqueue.h"
#include "../afs/nfsclient.h"
#include "../afs/afs_osidnlc.h"

extern afs_rwlock_t afs_xvcache;
extern afs_rwlock_t afs_xcbhash;

/* don't set CDirty in here because RPC is called synchronously */

#ifdef	AFS_OSF_ENV
afs_mkdir(ndp, attrs)
    struct nameidata *ndp;
    struct vattr *attrs; {
    register struct vcache *adp = (struct vcache *)ndp->ni_dvp;
    char *aname = ndp->ni_dent.d_name;
    register struct vcache **avcp = (struct vcache **)&(ndp->ni_vp);
    struct ucred *acred = ndp->ni_cred;
#else	/* AFS_OSF_ENV */
afs_mkdir(OSI_VC_ARG(adp), aname, attrs, avcp, acred)
    OSI_VC_DECL(adp);
    register struct vcache **avcp;
    char *aname;
    struct vattr *attrs;
    struct AFS_UCRED *acred; {
#endif
    struct vrequest treq;
    register afs_int32 code;
    register struct conn *tc;
    struct VenusFid newFid;
    register struct dcache *tdc;
    afs_int32 offset, len;
    register struct vcache *tvc;
    struct AFSStoreStatus InStatus;
    struct AFSFetchStatus OutFidStatus, OutDirStatus;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    afs_int32 now;
    XSTATS_DECLS
    OSI_VC_CONVERT(adp)

    AFS_STATCNT(afs_mkdir);
    afs_Trace2(afs_iclSetp, CM_TRACE_MKDIR, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_STRING, aname);

    if (code = afs_InitReq(&treq, acred)) return code;

    if (!afs_ENameOK(aname)) {
	code = EINVAL;
	goto done;
    }
    code = afs_VerifyVCache(adp, &treq);
    if (code) goto done;

    /** If the volume is read-only, return error without making an RPC to the
      * fileserver
      */
    if ( adp->states & CRO ) {
        code = EROFS;
        goto done;
    }

    InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE | AFS_SETGROUP;
    InStatus.ClientModTime = osi_Time();
    InStatus.UnixModeBits = attrs->va_mode & 0xffff;   /* only care about protection bits */
    InStatus.Group = (afs_int32)acred->cr_gid;
    tdc = afs_GetDCache(adp, 0, &treq, &offset, &len, 1);
    ObtainWriteLock(&adp->lock,153);
    do {
	tc = afs_Conn(&adp->fid, &treq, SHARED_LOCK);
	if (tc) {
          XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_MAKEDIR);
	  now = osi_Time();
#ifdef RX_ENABLE_LOCKS
	  AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	  code = RXAFS_MakeDir(tc->id, (struct AFSFid *) &adp->fid.Fid, aname,
			      &InStatus, (struct AFSFid *) &newFid.Fid,
			      &OutFidStatus, &OutDirStatus, &CallBack, &tsync);
#ifdef RX_ENABLE_LOCKS
	  AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
          XSTATS_END_TIME;
	    CallBack.ExpirationTime += now;
	    /* DON'T forget to Set the callback value... */
	}
	else code = -1;
    } while
      (afs_Analyze(tc, code, &adp->fid, &treq,
		   AFS_STATS_FS_RPCIDX_MAKEDIR, SHARED_LOCK, (struct cell *)0));

    if (code) {
	if (code < 0) {
	  ObtainWriteLock(&afs_xcbhash, 490);
	  afs_DequeueCallback(adp);
	  adp->states &= ~CStatd;
	  ReleaseWriteLock(&afs_xcbhash);
	  osi_dnlc_purgedp(adp);
	}
	ReleaseWriteLock(&adp->lock);
	if (tdc) afs_PutDCache(tdc);
	goto done;
    }
    /* otherwise, we should see if we can make the change to the dir locally */
    if (afs_LocalHero(adp, tdc, &OutDirStatus, 1)) {
        /* we can do it locally */
        code = afs_dir_Create(&tdc->f.inode, aname, &newFid.Fid);
        if (code) {
	    ZapDCE(tdc);	/* surprise error -- use invalid value */
	    DZap(&tdc->f.inode);
	}
    }
    if (tdc) {
	afs_PutDCache(tdc);
    }
    adp->m.LinkCount = OutDirStatus.LinkCount;
    newFid.Cell = adp->fid.Cell;
    newFid.Fid.Volume = adp->fid.Fid.Volume;
    ReleaseWriteLock(&adp->lock);
    /* now we're done with parent dir, create the real dir's cache entry */
    tvc = afs_GetVCache(&newFid, &treq, (afs_int32 *)0, (struct vcache*)0, 0);
    if (tvc) {
	code = 0;
	*avcp = tvc;
    }
    else code = ENOENT;
done:
#ifdef	AFS_OSF_ENV
    AFS_RELE(ndp->ni_dvp);
#endif	/* AFS_OSF_ENV */
    code = afs_CheckCode(code, &treq, 26);
    return code;
}


#ifdef	AFS_OSF_ENV
afs_rmdir(ndp)
    struct nameidata *ndp; {
    register struct vcache *adp = (struct vcache *)ndp->ni_dvp;
    char *aname = ndp->ni_dent.d_name;
    struct ucred *acred = ndp->ni_cred;
#else	/* AFS_OSF_ENV */
/* don't set CDirty in here because RPC is called synchronously */
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
afs_rmdir(OSI_VC_ARG(adp), aname, cdirp, acred)
    struct vnode *cdirp;
#else
afs_rmdir(adp, aname, acred)
#endif
    OSI_VC_DECL(adp);
    char *aname;
    struct AFS_UCRED *acred; {
#endif
    struct vrequest treq;
    register struct dcache *tdc;
    register struct vcache *tvc = (struct vcache *)0;
    register afs_int32 code;
    register struct conn *tc;
    afs_int32 offset, len;
    struct AFSFetchStatus OutDirStatus;
    struct AFSVolSync tsync;
    XSTATS_DECLS
    OSI_VC_CONVERT(adp)

    AFS_STATCNT(afs_rmdir);

    afs_Trace2(afs_iclSetp, CM_TRACE_RMDIR, ICL_TYPE_POINTER, adp, 
	       ICL_TYPE_STRING, aname);

    if (code = afs_InitReq(&treq, acred)) return code;
    code = afs_VerifyVCache(adp, &treq);
    if (code) goto done;

    /** If the volume is read-only, return error without making an RPC to the
      * fileserver
      */
    if ( adp->states & CRO ) {
        code = EROFS;
        goto done;
    }

    tdc	= afs_GetDCache(adp, 0,	&treq, &offset,	&len, 1);	/* test for error below */
    ObtainWriteLock(&adp->lock,154);
    if (tdc && (adp->states & CForeign)) {
	struct VenusFid unlinkFid;

	unlinkFid.Fid.Vnode = 0;
	code = afs_dir_Lookup(&tdc->f.inode, aname, &unlinkFid.Fid);
	if (code == 0) {	
	    afs_int32 cached=0;

	    unlinkFid.Cell = adp->fid.Cell;
	    unlinkFid.Fid.Volume = adp->fid.Fid.Volume;
	    if (unlinkFid.Fid.Unique == 0) {
		tvc = afs_LookupVCache(&unlinkFid, &treq, &cached, 
				       WRITE_LOCK, adp, aname);
	    } else {
		ObtainReadLock(&afs_xvcache);
		tvc = afs_FindVCache(&unlinkFid, 1, WRITE_LOCK, 
				     0, 1/* do xstats */);
		ReleaseReadLock(&afs_xvcache);
	    }
	}
    }

    do {
	tc = afs_Conn(&adp->fid, &treq, SHARED_LOCK);
	if (tc) {
          XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEDIR);
#ifdef RX_ENABLE_LOCKS
	  AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	    code = RXAFS_RemoveDir(tc->id, (struct AFSFid *) &adp->fid.Fid,
				   aname, &OutDirStatus, &tsync);
#ifdef RX_ENABLE_LOCKS
	  AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
          XSTATS_END_TIME;
	}
	else code = -1;
    } while
      (afs_Analyze(tc, code, &adp->fid, &treq,
		   AFS_STATS_FS_RPCIDX_REMOVEDIR, SHARED_LOCK, (struct cell *)0));

    if (code) {
	if (tdc) afs_PutDCache(tdc);
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
    if (afs_LocalHero(adp, tdc, &OutDirStatus, 1)) {
	/* we can do it locally */
	code = afs_dir_Delete(&tdc->f.inode, aname);
	if (code) { 
	    ZapDCE(tdc);	/* surprise error -- invalid value */
	    DZap(&tdc->f.inode);
	}
    }
    if (tdc) {
	afs_PutDCache(tdc);	/* drop ref count */
    }


    if (tvc) {
	osi_dnlc_purgedp (tvc);  /* get rid of any entries for this directory */
	afs_symhint_inval(tvc);
    } else
	osi_dnlc_remove (adp, aname, 0);

    if (tvc) {
	ObtainWriteLock(&tvc->lock,155);
	tvc->states &= ~CUnique;		/* For the dfs xlator */
	ReleaseWriteLock(&tvc->lock);
	afs_PutVCache(tvc, WRITE_LOCK);
    }
    ReleaseWriteLock(&adp->lock);
    /* don't worry about link count since dirs can not be hardlinked */
    code = 0;

done:
#ifdef	AFS_OSF_ENV
    afs_PutVCache(adp, 0);
    afs_PutVCache(ndp->ni_vp, 0);
#endif	/* AFS_OSF_ENV */
    code = afs_CheckCode(code, &treq, 27); 
    return code;
}
