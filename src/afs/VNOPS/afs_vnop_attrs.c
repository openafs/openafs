/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_vnop_attrs.c - setattr and getattr vnodeops
 *
 * Implements:
 * afs_CopyOutAttrs
 * afs_getattr
 * afs_VAttrToAS
 * afs_setattr
 *
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h" /* statistics */
#include "../afs/afs_cbqueue.h"
#include "../afs/nfsclient.h"
#include "../afs/afs_osidnlc.h"

extern afs_rwlock_t afs_xcbhash;
struct afs_exporter *afs_nfsexporter;
extern struct vcache *afs_globalVp;

/* copy out attributes from cache entry */
afs_CopyOutAttrs(avc, attrs)
    register struct vattr *attrs;
    register struct vcache *avc; {
    register struct volume *tvp;
    register struct cell *tcell;
    register afs_int32 i;

    AFS_STATCNT(afs_CopyOutAttrs);
#if	defined(AFS_MACH_ENV )
    attrs->va_mode = vType(avc) | (avc->m.Mode&~VFMT);
#else /* AFS_MACH_ENV */
    attrs->va_type = vType(avc);
#if defined(AFS_SGI_ENV) || defined(AFS_AIX32_ENV) || defined(AFS_SUN5_ENV)
    attrs->va_mode = (mode_t)(avc->m.Mode & 0xffff);
#else
    attrs->va_mode = avc->m.Mode;
#endif
#endif /* AFS_MACH_ENV */

    if (avc->m.Mode & (VSUID|VSGID)) {
	/* setuid or setgid, make sure we're allowed to run them from this cell */
	tcell = afs_GetCell(avc->fid.Cell, 0);
	if (tcell && (tcell->states & CNoSUID))
	    attrs->va_mode &= ~(VSUID|VSGID);
    }
    attrs->va_uid = avc->m.Owner;
    attrs->va_gid = avc->m.Group;   /* yeah! */
#if	defined(AFS_SUN56_ENV)
    attrs->va_fsid = avc->v.v_vfsp->vfs_fsid.val[0];
#else
#ifdef	AFS_SUN5_ENV
    /* XXX We try making this match the vfs's dev field  XXX */
    attrs->va_fsid = 1;
#else
#ifdef AFS_OSF_ENV
    attrs->va_fsid = avc->v.v_mount->m_stat.f_fsid.val[0]; 
#else
    attrs->va_fsid = 1;
#endif
#endif
#endif /* AFS_SUN56_ENV */
    if (avc->mvstat == 2) {
	tvp = afs_GetVolume(&avc->fid, 0, READ_LOCK);
	/* The mount point's vnode. */
        if (tvp) {
	    attrs->va_nodeid =
		tvp->mtpoint.Fid.Vnode + (tvp->mtpoint.Fid.Volume << 16);
	    if (FidCmp(&afs_rootFid, &avc->fid) && !attrs->va_nodeid)
		attrs->va_nodeid = 2;
	    afs_PutVolume(tvp, READ_LOCK);
	}
	else attrs->va_nodeid = 2;
    }
    else attrs->va_nodeid = avc->fid.Fid.Vnode + (avc->fid.Fid.Volume << 16);
    attrs->va_nodeid &= 0x7fffffff;	/* Saber C hates negative inode #s! */
    attrs->va_nlink = avc->m.LinkCount;
    attrs->va_size = avc->m.Length;
    attrs->va_atime.tv_sec = attrs->va_mtime.tv_sec = attrs->va_ctime.tv_sec =
	avc->m.Date;
    /* set microseconds to be dataversion # so that we approximate NFS-style
     * use of mtime as a dataversion #.  We take it mod 512K because
     * microseconds *must* be less than a million, and 512K is the biggest
     * power of 2 less than such.  DataVersions are typically pretty small
     * anyway, so the difference between 512K and 1000000 shouldn't matter
     * much, and "&" is a lot faster than "%".
     */
#if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV)
    attrs->va_atime.tv_nsec = attrs->va_mtime.tv_nsec =
	attrs->va_ctime.tv_nsec =
	    (hgetlo(avc->m.DataVersion) & 0x7ffff) * 1000;
#ifdef	AFS_AIX41_ENV
    attrs->va_blocksize = PAGESIZE;		/* XXX Was 8192 XXX */
#else
    attrs->va_blksize = PAGESIZE;		/* XXX Was 8192 XXX */
#endif
#else
    attrs->va_atime.tv_usec = attrs->va_mtime.tv_usec =
	attrs->va_ctime.tv_usec =
	    (hgetlo(avc->m.DataVersion) & 0x7ffff);
    attrs->va_blocksize = PAGESIZE;		/* XXX Was 8192 XXX */
#endif
#ifdef AFS_DEC_ENV
    /* Have to use real device #s in Ultrix, since that's how FS type is
     * encoded.  If rdev doesn't match Ultrix equivalent of statfs's rdev, then
     * "df ." doesn't work.
     */
    if (afs_globalVFS && afs_globalVFS->vfs_data)
	attrs->va_rdev = ((struct mount *)(afs_globalVFS->vfs_data))->m_dev;
    else
	attrs->va_rdev = 1;	/* better than nothing */
#else 
    attrs->va_rdev = 1;
#endif
    /*
     * Below return 0 (and not 1) blocks if the file is zero length. This conforms
     * better with the other filesystems that do return 0.	
     */
#ifdef	AFS_OSF_ENV
#ifdef	va_size_rsv
    attrs->va_size_rsv = 0;
#endif
/* XXX do this */
/*    attrs->va_gen = avc->m.DataVersion;*/
    attrs->va_flags = 0;
#endif	/* AFS_OSF_ENV */

#if !defined(AFS_OSF_ENV)
#if !defined(AFS_HPUX_ENV)
#ifdef	AFS_SUN5_ENV
    attrs->va_nblocks = (attrs->va_size? ((attrs->va_size + 1023)>>10) << 1 : 0);
#else
#if defined(AFS_SGI_ENV)
    attrs->va_blocks = BTOBB(attrs->va_size);
#else
    attrs->va_blocks = (attrs->va_size? ((attrs->va_size + 1023)>>10) << 1 : 0);
#endif
#endif
#else /* !defined(AFS_HPUX_ENV) */
    attrs->va_blocks = (attrs->va_size? ((attrs->va_size + 1023)>>10) : 0);
#endif /* !defined(AFS_HPUX_ENV) */
#else	/* ! AFS_OSF_ENV */
    attrs->va_bytes = (attrs->va_size? (attrs->va_size + 1023) : 1024);
#ifdef	va_bytes_rsv
    attrs->va_bytes_rsv = -1;
#endif
#endif	/* ! AFS_OSF_ENV */

#ifdef AFS_LINUX22_ENV
    /* And linux has it's own stash as well. */
    vattr2inode((struct inode*)avc, attrs);
#endif
    return 0;
}



#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
afs_getattr(OSI_VC_ARG(avc), attrs, flags, acred)
    int flags;
#else
afs_getattr(OSI_VC_ARG(avc), attrs, acred)
#endif
    OSI_VC_DECL(avc);
    struct vattr *attrs;
    struct AFS_UCRED *acred; 
{
    afs_int32 code;
    struct vrequest treq;
    extern struct unixuser *afs_FindUser();
    struct unixuser *au;
    int inited = 0;
    OSI_VC_CONVERT(avc)

    AFS_STATCNT(afs_getattr);
    afs_Trace2(afs_iclSetp, CM_TRACE_GETATTR, ICL_TYPE_POINTER, avc, 
	       ICL_TYPE_INT32, avc->m.Length);

#if defined(AFS_SUN5_ENV)
    if (flags & ATTR_HINT) {
       code = afs_CopyOutAttrs(avc, attrs);
       return code;
    }
#endif

#if defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
    afs_BozonLock(&avc->pvnLock, avc);
#endif

    if (afs_shuttingdown) return EIO;

    if (!(avc->states & CStatd)) {
      if (!(code = afs_InitReq(&treq, acred))) {
	code = afs_VerifyVCache2(avc, &treq);
	inited = 1;
      }
    }
    else code = 0;

#if defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
    if (code == 0) osi_FlushPages(avc, acred);
    afs_BozonUnlock(&avc->pvnLock, avc);
#endif


    if (code == 0) {
	osi_FlushText(avc); /* only needed to flush text if text locked last time */
	code = afs_CopyOutAttrs(avc, attrs);

	if (afs_nfsexporter) {
	  if (!inited) {
	    if (code = afs_InitReq(&treq, acred))
	      return code;
	     inited = 1;
	  }
	  if (AFS_NFSXLATORREQ(acred)) {
	      if ((vType(avc) != VDIR) &&
		  !afs_AccessOK(avc, PRSFS_READ, &treq,
				CHECK_MODE_BITS|CMB_ALLOW_EXEC_AS_READ)) {
		  return EACCES;
	      }
	      if (avc->mvstat == 2) {
#if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV)
		  attrs->va_mtime.tv_nsec += ((++avc->xlatordv) * 1000); 
#else
		  attrs->va_mtime.tv_usec += ++avc->xlatordv; 
#endif
	      }
	  }
	  if (au = afs_FindUser(treq.uid, -1, READ_LOCK)) {
	    register struct afs_exporter *exporter = au->exporter;

	    if (exporter && !(afs_nfsexporter->exp_states & EXP_UNIXMODE)) {
		unsigned int ubits;
		/*
		 *  If the remote user wishes to enforce default Unix mode semantics,
		 *  like in the nfs exporter case, we OR in the user bits
		 *  into the group and other bits. We need to do this
		 *  because there is no RFS_ACCESS call and thus nfs
		 *  clients implement nfs_access by interpreting the 
		 *  mode bits in the traditional way, which of course
		 *  loses with afs.
		 */
		ubits = (attrs->va_mode & 0700) >> 6;
		attrs->va_mode = attrs->va_mode | ubits | (ubits << 3);
		/* If it's the root of AFS, replace the inode number with the
		 * inode number of the mounted on directory; otherwise this 
		 * confuses getwd()... */
#ifdef AFS_LINUX22_ENV
		if (avc == afs_globalVp) {
		    struct inode *ip = avc->v.i_sb->s_root->d_inode;
		    attrs->va_nodeid = ip->i_ino;
		}
#else
		if (avc->v.v_flag & VROOT) {
		    struct vnode *vp = (struct vnode *)avc;

		    vp = vp->v_vfsp->vfs_vnodecovered;
		    if (vp) {	/* Ignore weird failures */
#ifdef AFS_SGI62_ENV
			attrs->va_nodeid = VnodeToIno(vp);
#else
			struct inode *ip;
			
			ip = (struct inode *) VTOI(vp);
			if (ip)	/* Ignore weird failures */
			    attrs->va_nodeid = ip->i_number;
#endif
		    }
		}
#endif /* AFS_LINUX22_ENV */
	      }
	    afs_PutUser(au, READ_LOCK);
	  }
	}
    }
    if (!code)
      return 0;
    code = afs_CheckCode(code, &treq, 14);
    return code;
}

/* convert a Unix request into a status store request */
afs_VAttrToAS(avc, av, as)
register struct vcache *avc;
register struct vattr *av;
register struct AFSStoreStatus *as; {
    register int mask;
    mask = 0;
    AFS_STATCNT(afs_VAttrToAS);
#if	defined(AFS_AIX_ENV)
/* Boy, was this machine dependent bogosity hard to swallow????.... */
    if (av->va_mode != -1) {
#else
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX22_ENV)
    if (av->va_mask & AT_MODE) {
#else
    if (av->va_mode != ((unsigned short)-1)) {
#endif
#endif
	mask |= AFS_SETMODE;
	as->UnixModeBits = av->va_mode & 0xffff;
	if (avc->states & CForeign) {
	    ObtainWriteLock(&avc->lock,127);
	    afs_FreeAllAxs(&(avc->Access));
	    ReleaseWriteLock(&avc->lock);
	}
    }

#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX22_ENV)
    if (av->va_mask & AT_GID) {
#else
#if (defined(AFS_HPUX_ENV) || defined(AFS_SUN_ENV))
#if	defined(AFS_HPUX102_ENV)
    if (av->va_gid != GID_NO_CHANGE) {
#else
    if (av->va_gid != ((unsigned short)-1)) {
#endif
#else
    if (av->va_gid != -1) {
#endif
#endif /* AFS_SUN5_ENV */
	mask |= AFS_SETGROUP;
	as->Group = av->va_gid;
    }

#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX22_ENV)
    if (av->va_mask & AT_UID) {
#else
#if (defined(AFS_HPUX_ENV) || defined(AFS_SUN_ENV))
#if	defined(AFS_HPUX102_ENV)
    if (av->va_uid != UID_NO_CHANGE) {
#else
    if (av->va_uid != ((unsigned short)-1)) {
#endif
#else
    if (av->va_uid != -1) {
#endif
#endif /* AFS_SUN5_ENV */
	mask |= AFS_SETOWNER;
	as->Owner = av->va_uid;
    }
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX22_ENV)
    if (av->va_mask & AT_MTIME) {
#else
    if (av->va_mtime.tv_sec != -1) {
#endif
	mask |= AFS_SETMODTIME;
#ifndef	AFS_SGI_ENV
#if	defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV)
	if (av->va_mtime.tv_nsec == -1)
#else
	if (av->va_mtime.tv_usec == -1)
#endif
	    as->ClientModTime = osi_Time();	/* special Sys V compat hack for Suns */
	else
#endif
	    as->ClientModTime = av->va_mtime.tv_sec;
    }
    as->Mask = mask;
    return 0;
}

/* We don't set CDirty bit in avc->states because setattr calls WriteVCache
 * synchronously, therefore, it's not needed.
 */
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
afs_setattr(OSI_VC_ARG(avc), attrs, flags, acred)
    int flags;
#else
afs_setattr(avc, attrs, acred)
#endif
    OSI_VC_DECL(avc);
    register struct vattr *attrs;
    struct AFS_UCRED *acred; {
    struct vrequest treq;
    struct AFSStoreStatus astat;
    register afs_int32 code;
    OSI_VC_CONVERT(avc)

    AFS_STATCNT(afs_setattr);
    afs_Trace2(afs_iclSetp, CM_TRACE_SETATTR, ICL_TYPE_POINTER, avc, 
	       ICL_TYPE_INT32, avc->m.Length);
    if (code = afs_InitReq(&treq, acred)) return code;
    if (avc->states & CRO) {
	code=EROFS;
	goto done;
    }
#if defined(AFS_SGI_ENV)
    /* ignore ATTR_LAZY calls - they are really only for keeping
     * the access/mtime of mmaped files up to date
     */
    if (flags & ATTR_LAZY)
	goto done;
#endif
#ifndef AFS_DEC_ENV
    /* if file size has changed, we need write access, otherwise (e.g.
     * chmod) give it a shot; if it fails, we'll discard the status
     * info.
     *
     * Note that Ultrix actually defines ftruncate of a file you have open to
     * be O.K., and does the proper access checks itself in the truncate
     * path (unlike BSD or SUNOS), so we skip this check for Ultrix.
     *
     */
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX22_ENV)
    if (attrs->va_mask & AT_SIZE) {
#else
#ifdef	AFS_OSF_ENV
    if (attrs->va_size != VNOVAL) {
#else
#ifdef	AFS_AIX41_ENV
    if (attrs->va_size != -1) {
#else
    if (attrs->va_size != ~0) {
#endif
#endif
#endif
	if (!afs_AccessOK(avc, PRSFS_WRITE, &treq, DONT_CHECK_MODE_BITS)) {
	    code = EACCES;
	    goto done;
	}
    }
#endif

    afs_VAttrToAS(avc, attrs, &astat);	/* interpret request */
    code = 0;
#if defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
    afs_BozonLock(&avc->pvnLock, avc);
#endif
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (AFS_NFSXLATORREQ(acred)) {
	avc->execsOrWriters++;
    }
#endif

#if defined(AFS_SGI_ENV)
    AFS_RWLOCK((vnode_t *)avc, VRWLOCK_WRITE);
#endif
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX22_ENV)
    if (attrs->va_mask & AT_SIZE) {
#else
#if	defined(AFS_OSF_ENV)
    if (attrs->va_size != VNOVAL) {
#else	/* AFS_OSF_ENV */
    if (attrs->va_size != -1) {
#endif
#endif
	ObtainWriteLock(&avc->lock,128);
	avc->states |= CDirty;
	code = afs_TruncateAllSegments(avc, (afs_int32)attrs->va_size, &treq, acred);
	/* if date not explicitly set by this call, set it ourselves, since we
	 * changed the data */
	if (!(astat.Mask & AFS_SETMODTIME)) {
	    astat.Mask |= AFS_SETMODTIME;
	    astat.ClientModTime = osi_Time();
	}
	if (code == 0) {
          if ( ( (avc->execsOrWriters <= 0 ) && (avc->states & CCreating) == 0)
                || (  avc->execsOrWriters == 1 && AFS_NFSXLATORREQ(acred)) ) {
	    code = afs_StoreAllSegments(avc, &treq, AFS_ASYNC);
	    if (!code)
	      avc->states &= ~CDirty;
	  }
	} else
	  avc->states &= ~CDirty;

	ReleaseWriteLock(&avc->lock);
	hzero(avc->flushDV);
	osi_FlushText(avc);	/* do this after releasing all locks */
#ifdef AFS_DEC_ENV
	/* in case we changed the size here, propagate it to gp->g_size */
	afs_gfshack((struct gnode *)avc);
#endif
    }
    if (code == 0) {
	ObtainSharedLock(&avc->lock,16);	/* lock entry */
	code = afs_WriteVCache(avc, &astat, &treq);    /* send request */
	ReleaseSharedLock(&avc->lock);	/* release lock */
    }
    if (code) {
       ObtainWriteLock(&afs_xcbhash, 487);
       afs_DequeueCallback(avc);
       avc->states &= ~CStatd;	    
       ReleaseWriteLock(&afs_xcbhash);
       if (avc->fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
	   osi_dnlc_purgedp(avc);
       /* error?  erase any changes we made to vcache entry */
     }

#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (AFS_NFSXLATORREQ(acred)) {
	avc->execsOrWriters--;
    }
#endif
#if defined(AFS_SUN_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SUN5_ENV)
    afs_BozonUnlock(&avc->pvnLock, avc);
#endif
#if defined(AFS_SGI_ENV)
    AFS_RWUNLOCK((vnode_t *)avc, VRWLOCK_WRITE);
#endif
done:
    code = afs_CheckCode(code, &treq, 15);
    return code;
}
