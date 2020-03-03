/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2003 Apple Computer, Inc.
 */

/*
 * afs_vnop_attrs.c - setattr and getattr vnodeops
 *
 * Implements:
 * afs_CopyOutAttrs
 * afs_getattr
 * afs_VAttrToAS
 * afs_setattr
 * afs_CreateAttr
 * afs_DestroyAttr
 *
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"

extern afs_rwlock_t afs_xcbhash;
struct afs_exporter *afs_nfsexporter;
extern struct vcache *afs_globalVp;
#if defined(AFS_HPUX110_ENV)
extern struct vfs *afs_globalVFS;
#endif


/* copy out attributes from cache entry */
int
afs_CopyOutAttrs(struct vcache *avc, struct vattr *attrs)
{
    struct volume *tvp;
    struct cell *tcell;
#if defined(AFS_FBSD_ENV) || defined(AFS_DFBSD_ENV)
    struct vnode *vp = AFSTOV(avc);
#endif
    int fakedir = 0;

    AFS_STATCNT(afs_CopyOutAttrs);
    if (afs_fakestat_enable && avc->mvstat == AFS_MVSTAT_MTPT)
	fakedir = 1;
    attrs->va_type = fakedir ? VDIR : vType(avc);
#if defined(AFS_SGI_ENV) || defined(AFS_AIX32_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_DARWIN_ENV)
    attrs->va_mode = fakedir ? S_IFDIR | 0755 : (mode_t) (avc->f.m.Mode & 0xffff);
#else
    attrs->va_mode = fakedir ? VDIR | 0755 : avc->f.m.Mode;
#endif

    if (avc->f.m.Mode & (VSUID | VSGID)) {
	/* setuid or setgid, make sure we're allowed to run them from this cell */
	tcell = afs_GetCell(avc->f.fid.Cell, 0);
	if (tcell && (tcell->states & CNoSUID))
	    attrs->va_mode &= ~(VSUID | VSGID);
    }
#if defined(AFS_DARWIN_ENV)
    {
	if (!afs_darwin_realmodes) {
	    /* Mac OS X uses the mode bits to determine whether a file or
	     * directory is accessible, and believes them, even though under
	     * AFS they're almost assuredly wrong, especially if the local uid
	     * does not match the AFS ID.  So we set the mode bits
	     * conservatively.
	     */
	    if (S_ISDIR(attrs->va_mode)) {
		/* all access bits need to be set for directories, since even
		 * a mode 0 directory can still be used normally.
		 */
		attrs->va_mode |= ACCESSPERMS;
	    } else {
		/* for other files, replicate the user bits to group and other */
		mode_t ubits = (attrs->va_mode & S_IRWXU) >> 6;
		attrs->va_mode |= ubits | (ubits << 3);
	    }
	}
    }
#endif /* AFS_DARWIN_ENV */
    attrs->va_uid = fakedir ? 0 : avc->f.m.Owner;
    attrs->va_gid = fakedir ? 0 : avc->f.m.Group;	/* yeah! */
#if defined(AFS_SUN5_ENV)
    attrs->va_fsid = AFSTOV(avc)->v_vfsp->vfs_fsid.val[0];
#elif defined(AFS_DARWIN80_ENV)
    VATTR_RETURN(attrs, va_fsid, vfs_statfs(vnode_mount(AFSTOV(avc)))->f_fsid.val[0]);
#elif defined(AFS_DARWIN_ENV)
    attrs->va_fsid = avc->v->v_mount->mnt_stat.f_fsid.val[0];
#else /* ! AFS_DARWIN_ENV */
    attrs->va_fsid = 1;
#endif 
    if (avc->mvstat == AFS_MVSTAT_ROOT) {
	tvp = afs_GetVolume(&avc->f.fid, 0, READ_LOCK);
	/* The mount point's vnode. */
	if (tvp) {
	    attrs->va_nodeid =
	      afs_calc_inum(tvp->mtpoint.Cell,
			    tvp->mtpoint.Fid.Volume,
			    tvp->mtpoint.Fid.Vnode);
	    if (FidCmp(&afs_rootFid, &avc->f.fid) && !attrs->va_nodeid)
		attrs->va_nodeid = 2;
	    afs_PutVolume(tvp, READ_LOCK);
	} else
	    attrs->va_nodeid = 2;
    } else
	attrs->va_nodeid = 
	      afs_calc_inum(avc->f.fid.Cell,
			    avc->f.fid.Fid.Volume,
			    avc->f.fid.Fid.Vnode);
    attrs->va_nodeid &= 0x7fffffff;	/* Saber C hates negative inode #s! */
    attrs->va_nlink = fakedir ? 100 : avc->f.m.LinkCount;
    attrs->va_size = fakedir ? 4096 : avc->f.m.Length;
#if defined(AFS_FBSD_ENV) || defined(AFS_DFBSD_ENV)
        vnode_pager_setsize(vp, (u_long) attrs->va_size);
#endif
    attrs->va_atime.tv_sec = attrs->va_mtime.tv_sec = attrs->va_ctime.tv_sec =
	fakedir ? 0 : (int)avc->f.m.Date;
    /* set microseconds to be dataversion # so that we approximate NFS-style
     * use of mtime as a dataversion #.  We take it mod 512K because
     * microseconds *must* be less than a million, and 512K is the biggest
     * power of 2 less than such.  DataVersions are typically pretty small
     * anyway, so the difference between 512K and 1000000 shouldn't matter
     * much, and "&" is a lot faster than "%".
     */
#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
    /* nfs on these systems puts an 0 in nsec and stores the nfs usec (aka 
     * dataversion) in va_gen */

    attrs->va_atime.tv_nsec = attrs->va_mtime.tv_nsec =
	attrs->va_ctime.tv_nsec = 0;
    attrs->va_gen = hgetlo(avc->f.m.DataVersion);
#elif defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV) || defined(AFS_OBSD_ENV) || defined(AFS_NBSD_ENV) || defined(AFS_LINUX26_ENV)
    attrs->va_atime.tv_nsec = attrs->va_mtime.tv_nsec =
	attrs->va_ctime.tv_nsec =
	(hgetlo(avc->f.m.DataVersion) & 0x7ffff) * 1000;
#else
    attrs->va_atime.tv_usec = attrs->va_mtime.tv_usec =
	attrs->va_ctime.tv_usec = (hgetlo(avc->f.m.DataVersion) & 0x7ffff);
#endif
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    attrs->va_flags = 0;
#endif
#if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)
    attrs->va_blksize = AFS_BLKSIZE;	/* XXX Was 8192 XXX */
#else
    attrs->va_blocksize = AFS_BLKSIZE;	/* XXX Was 8192 XXX */
#endif
    attrs->va_rdev = 1;
#if defined(AFS_HPUX110_ENV)
    if (afs_globalVFS)
	attrs->va_fstype = afs_globalVFS->vfs_mtype;
#endif

    /*
     * Below return 0 (and not 1) blocks if the file is zero length. This conforms
     * better with the other filesystems that do return 0.      
     */
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    attrs->va_bytes = (attrs->va_size ? (attrs->va_size + 1023) : 1024);
#ifdef	va_bytes_rsv
    attrs->va_bytes_rsv = -1;
#endif
#elif defined(AFS_HPUX_ENV)
    attrs->va_blocks = (attrs->va_size ? ((attrs->va_size + 1023)>>10) : 0);
#elif defined(AFS_SGI_ENV)
    attrs->va_blocks = BTOBB(attrs->va_size);
#elif defined(AFS_SUN5_ENV)
    attrs->va_nblocks = (attrs->va_size ? ((attrs->va_size + 1023)>>10)<<1:0);
#else /* everything else */
    attrs->va_blocks = (attrs->va_size ? ((attrs->va_size + 1023)>>10)<<1:0);
#endif
    return 0;
}



#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
int
afs_getattr(OSI_VC_DECL(avc), struct vattr *attrs, int flags,
	    afs_ucred_t *acred)
#else
int
afs_getattr(OSI_VC_DECL(avc), struct vattr *attrs, afs_ucred_t *acred)
#endif
{
    afs_int32 code;
    struct vrequest *treq = NULL;
    struct unixuser *au;
    int inited = 0;
    OSI_VC_CONVERT(avc);

    AFS_STATCNT(afs_getattr);
    afs_Trace2(afs_iclSetp, CM_TRACE_GETATTR, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->f.m.Length));

    if (afs_fakestat_enable && avc->mvstat == AFS_MVSTAT_MTPT) {
	struct afs_fakestat_state fakestat;
	struct vrequest *ureq = NULL;

	code = afs_CreateReq(&ureq, acred);
	if (code) {
	    return code;
	}
	afs_InitFakeStat(&fakestat);
	code = afs_TryEvalFakeStat(&avc, &fakestat, ureq);
	if (code) {
	    afs_PutFakeStat(&fakestat);
	    afs_DestroyReq(ureq);
	    return code;
	}

	code = afs_CopyOutAttrs(avc, attrs);
	afs_PutFakeStat(&fakestat);
	afs_DestroyReq(ureq);
	return code;
    }
#if defined(AFS_SUN5_ENV)
    if (flags & ATTR_HINT) {
	code = afs_CopyOutAttrs(avc, attrs);
	return code;
    }
#endif
#if defined(AFS_DARWIN_ENV) && !defined(AFS_DARWIN80_ENV)
    if (avc->f.states & CUBCinit) {
	code = afs_CopyOutAttrs(avc, attrs);
	return code;
    }
#endif

    AFS_DISCON_LOCK();

    if (afs_shuttingdown != AFS_RUNNING) {
	AFS_DISCON_UNLOCK();
	return EIO;
    }

    if (!(avc->f.states & CStatd)) {
	if (!(code = afs_CreateReq(&treq, acred))) {
	    code = afs_VerifyVCache2(avc, treq);
	    inited = 1;
	}
    } else
	code = 0;

#if defined(AFS_SUN5_ENV)
    if (code == 0)
	osi_FlushPages(avc, acred);
#endif

    if (code == 0) {
	osi_FlushText(avc);	/* only needed to flush text if text locked last time */
	code = afs_CopyOutAttrs(avc, attrs);

	if (afs_nfsexporter) {
	    if (!inited) {
		if ((code = afs_CreateReq(&treq, acred))) {
		    return code;
		}
		inited = 1;
	    }
	    if (AFS_NFSXLATORREQ(acred)) {
		if ((vType(avc) != VDIR)
		    && !afs_AccessOK(avc, PRSFS_READ, treq,
				     CHECK_MODE_BITS |
				     CMB_ALLOW_EXEC_AS_READ)) {
		    afs_DestroyReq(treq);
		    return EACCES;
		}
	    }
	    if ((au = afs_FindUser(treq->uid, -1, READ_LOCK))) {
		struct afs_exporter *exporter = au->exporter;

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
			struct inode *ip = AFSTOV(avc)->i_sb->s_root->d_inode;
			attrs->va_nodeid = ip->i_ino;	/* VTOI()? */
		    }
#else
		    if (
#if defined(AFS_DARWIN_ENV)
			vnode_isvroot(AFSTOV(avc))
#elif defined(AFS_NBSD50_ENV)
			AFSTOV(avc)->v_vflag & VV_ROOT
#else
			AFSTOV(avc)->v_flag & VROOT
#endif
			) {
			struct vnode *vp = AFSTOV(avc);

#ifdef AFS_DARWIN80_ENV
			/* XXX vp = vnode_mount(vp)->mnt_vnodecovered; */
			vp = 0;
#else
			vp = vp->v_vfsp->vfs_vnodecovered;
			if (vp) {	/* Ignore weird failures */
#ifdef AFS_SGI62_ENV
			    attrs->va_nodeid = VnodeToIno(vp);
#else
			    struct inode *ip;

			    ip = (struct inode *)VTOI(vp);
			    if (ip)	/* Ignore weird failures */
				attrs->va_nodeid = ip->i_number;
#endif
			}
#endif
		    }
#endif /* AFS_LINUX22_ENV */
		}
		afs_PutUser(au, READ_LOCK);
	    }
	}
    }

    AFS_DISCON_UNLOCK();

    if (!code) {
	afs_DestroyReq(treq);
	return 0;
    }
    code = afs_CheckCode(code, treq, 14);
    afs_DestroyReq(treq);
    return code;
}

/* convert a Unix request into a status store request */
int
afs_VAttrToAS(struct vcache *avc, struct vattr *av,
	      struct AFSStoreStatus *as)
{
    int mask;
    mask = 0;

    AFS_STATCNT(afs_VAttrToAS);
#if     defined(AFS_DARWIN80_ENV)
    if (VATTR_IS_ACTIVE(av, va_mode)) {
#elif	defined(AFS_AIX_ENV)
/* Boy, was this machine dependent bogosity hard to swallow????.... */
    if (av->va_mode != -1) {
#elif	defined(AFS_LINUX22_ENV) || defined(UKERNEL)
    if (av->va_mask & ATTR_MODE) {
#elif	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (av->va_mask & AT_MODE) {
#elif	defined(AFS_XBSD_ENV)
    if (av->va_mode != (mode_t)VNOVAL) {
#else
    if (av->va_mode != ((unsigned short)-1)) {
#endif
	mask |= AFS_SETMODE;
	as->UnixModeBits = av->va_mode & 0xffff;
	if (avc->f.states & CForeign) {
	    ObtainWriteLock(&avc->lock, 127);
	    afs_FreeAllAxs(&(avc->Access));
	    ReleaseWriteLock(&avc->lock);
	}
    }
#if     defined(AFS_DARWIN80_ENV)
    if (VATTR_IS_ACTIVE(av, va_gid)) {
#elif defined(AFS_LINUX22_ENV) || defined(UKERNEL)
    if (av->va_mask & ATTR_GID) {
#elif defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (av->va_mask & AT_GID) {
#elif defined(AFS_HPUX_ENV)
#if	defined(AFS_HPUX102_ENV)
    if (av->va_gid != GID_NO_CHANGE) {
#else
    if (av->va_gid != ((unsigned short)-1)) {
#endif
#elif	defined(AFS_XBSD_ENV)
    if (av->va_gid != (gid_t)VNOVAL) {
#else
    if (av->va_gid != -1) {
#endif /* AFS_LINUX22_ENV */
	mask |= AFS_SETGROUP;
	as->Group = av->va_gid;
    }
#if     defined(AFS_DARWIN80_ENV)
    if (VATTR_IS_ACTIVE(av, va_uid)) {
#elif defined(AFS_LINUX22_ENV) || defined(UKERNEL)
    if (av->va_mask & ATTR_UID) {
#elif defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (av->va_mask & AT_UID) {
#elif defined(AFS_HPUX_ENV)
#if	defined(AFS_HPUX102_ENV)
    if (av->va_uid != UID_NO_CHANGE) {
#elif	defined(AFS_XBSD_ENV)
    if (av->va_uid != (uid_t)VNOVAL) {
#else
    if (av->va_uid != ((unsigned short)-1)) {
#endif
#else
    if (av->va_uid != -1) {
#endif /* AFS_LINUX22_ENV */
	mask |= AFS_SETOWNER;
	as->Owner = av->va_uid;
    }
#if     defined(AFS_DARWIN80_ENV)
    if (VATTR_IS_ACTIVE(av, va_modify_time)) {
#elif	defined(AFS_LINUX22_ENV) || defined(UKERNEL)
    if (av->va_mask & ATTR_MTIME) {
#elif	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (av->va_mask & AT_MTIME) {
#else
    if (av->va_mtime.tv_sec != -1) {
#endif
	mask |= AFS_SETMODTIME;
#ifndef	AFS_SGI_ENV
#if	defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV) || defined(AFS_LINUX26_ENV)
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

/* We don't set CDirty bit in avc->f.states because setattr calls WriteVCache
 * synchronously, therefore, it's not needed.
 */
#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
int
afs_setattr(OSI_VC_DECL(avc), struct vattr *attrs, int flags,
	    afs_ucred_t *acred)
#else
int
afs_setattr(OSI_VC_DECL(avc), struct vattr *attrs,
	    afs_ucred_t *acred)
#endif
{
    struct vrequest *treq = NULL;
    struct AFSStoreStatus astat;
    afs_int32 code;
#if defined(AFS_FBSD_ENV) || defined(AFS_DFBSD_ENV)
    struct vnode *vp = AFSTOV(avc);
#endif
    struct afs_fakestat_state fakestate;
    OSI_VC_CONVERT(avc);

    AFS_STATCNT(afs_setattr);
#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX22_ENV)
    afs_Trace4(afs_iclSetp, CM_TRACE_SETATTR, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, attrs->va_mask, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(attrs->va_size), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->f.m.Length));
#else
    afs_Trace4(afs_iclSetp, CM_TRACE_SETATTR, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, attrs->va_mode, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(attrs->va_size), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->f.m.Length));
#endif
    if ((code = afs_CreateReq(&treq, acred)))
	return code;

    memset(&astat, 0, sizeof(astat));

    AFS_DISCON_LOCK();

    afs_InitFakeStat(&fakestate);
    code = afs_EvalFakeStat(&avc, &fakestate, treq);
    if (code)
	goto done;

    if (avc->f.states & CRO) {
	code = EROFS;
	goto done;
    }
#if defined(AFS_SGI_ENV)
    /* ignore ATTR_LAZY calls - they are really only for keeping
     * the access/mtime of mmaped files up to date
     */
    if (flags & ATTR_LAZY)
	goto done;
#endif
    /* if file size has changed, we need write access, otherwise (e.g.
     * chmod) give it a shot; if it fails, we'll discard the status
     * info.
     */
#if	defined(AFS_DARWIN80_ENV)
    if (VATTR_IS_ACTIVE(attrs, va_data_size)) {
#elif	defined(AFS_LINUX22_ENV) || defined(UKERNEL)
    if (attrs->va_mask & ATTR_SIZE) {
#elif	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (attrs->va_mask & AT_SIZE) {
#elif	defined(AFS_XBSD_ENV)
    if (attrs->va_size != VNOVAL) {
#elif	defined(AFS_AIX41_ENV)
    if (attrs->va_size != -1) {
#else
    if (attrs->va_size != ~0) {
#endif
	if (!afs_AccessOK(avc, PRSFS_WRITE, treq, DONT_CHECK_MODE_BITS)) {
	    code = EACCES;
	    goto done;
	}
    }

    if (AFS_IS_DISCONNECTED && !AFS_IS_DISCON_RW) {
        code = ENETDOWN;
        goto done;
    }

    afs_VAttrToAS(avc, attrs, &astat);	/* interpret request */
    code = 0;
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (AFS_NFSXLATORREQ(acred)) {
	avc->execsOrWriters++;
    }
#endif

#if defined(AFS_SGI_ENV)
    AFS_RWLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#endif
#if	defined(AFS_DARWIN80_ENV)
    if (VATTR_IS_ACTIVE(attrs, va_data_size)) {
#elif	defined(AFS_LINUX22_ENV) || defined(UKERNEL)
    if (attrs->va_mask & ATTR_SIZE) {
#elif	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (attrs->va_mask & AT_SIZE) {
#elif	defined(AFS_XBSD_ENV)
    if (attrs->va_size != VNOVAL) {
#elif	defined(AFS_AIX41_ENV)
    if (attrs->va_size != -1) {
#else
    if (attrs->va_size != ~0) {
#endif
	afs_size_t tsize = attrs->va_size;
	ObtainWriteLock(&avc->lock, 128);
	avc->f.states |= CDirty;

        if (AFS_IS_DISCONNECTED && tsize >=avc->f.m.Length) {
	    /* If we're growing the file, and we're disconnected, we need
 	     * to make the relevant dcache chunks appear ourselves. */
	    code = afs_ExtendSegments(avc, tsize, treq);
	} else {
	    code = afs_TruncateAllSegments(avc, tsize, treq, acred);
	}
#ifdef AFS_LINUX26_ENV
	/* We must update the Linux kernel's idea of file size as soon as
	 * possible, to avoid racing with delayed writepages delivered by
	 * pdflush */
	if (code == 0)
	    i_size_write(AFSTOV(avc), tsize);
#endif
#if defined(AFS_FBSD_ENV) || defined(AFS_DFBSD_ENV)
        vnode_pager_setsize(vp, (u_long) tsize);
#endif
	/* if date not explicitly set by this call, set it ourselves, since we
	 * changed the data */
	if (!(astat.Mask & AFS_SETMODTIME)) {
	    astat.Mask |= AFS_SETMODTIME;
	    astat.ClientModTime = osi_Time();
	}

	if (code == 0) {
	    if (((avc->execsOrWriters <= 0) && (avc->f.states & CCreating) == 0)
		|| (avc->execsOrWriters == 1 && AFS_NFSXLATORREQ(acred))) {

		/* Store files now if not disconnected. */
		/* XXX: AFS_IS_DISCON_RW handled. */
		if (!AFS_IS_DISCONNECTED) {
			code = afs_StoreAllSegments(avc, treq, AFS_ASYNC);
			if (!code)
		    		avc->f.states &= ~CDirty;
		}
	    }
	} else
	    avc->f.states &= ~CDirty;

	ReleaseWriteLock(&avc->lock);
	hzero(avc->flushDV);
	osi_FlushText(avc);	/* do this after releasing all locks */
    }
    
    if (!AFS_IS_DISCONNECTED) {
        if (code == 0) {
	    ObtainSharedLock(&avc->lock, 16);	/* lock entry */
	    code = afs_WriteVCache(avc, &astat, treq);	/* send request */
	    ReleaseSharedLock(&avc->lock);	/* release lock */
        }
        if (code) {
	    /* error?  erase any changes we made to vcache entry */
	    afs_StaleVCache(avc);
        }
    } else {
	ObtainSharedLock(&avc->lock, 712);
	/* Write changes locally. */
	code = afs_WriteVCacheDiscon(avc, &astat, attrs);
	ReleaseSharedLock(&avc->lock);
    }		/* if (!AFS_IS_DISCONNECTED) */

#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (AFS_NFSXLATORREQ(acred)) {
	avc->execsOrWriters--;
    }
#endif
#if defined(AFS_SGI_ENV)
    AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#endif
  done:
    afs_PutFakeStat(&fakestate);

    AFS_DISCON_UNLOCK();
    code = afs_CheckCode(code, treq, 15);
    afs_DestroyReq(treq);
    return code;
}

/*!
 * Allocate a vattr.
 *
 * \note The caller must free the allocated vattr with
 *       afs_DestroyAttr() if this function returns successfully (zero).
 *
 * \note The GLOCK must be held on platforms which require the GLOCK
 *       for osi_AllocSmallSpace() and osi_FreeSmallSpace().
 *
 * \param[out] out   address of the vattr pointer
 * \return     0 on success
 */
int
afs_CreateAttr(struct vattr **out)
{
    struct vattr *vattr = NULL;

    if (!out) {
	return EINVAL;
    }
    vattr = osi_AllocSmallSpace(sizeof(struct vattr));
    if (!vattr) {
	return ENOMEM;
    }
    memset(vattr, 0, sizeof(struct vattr));
    *out = vattr;
    return 0;
}

/*!
 * Deallocate a vattr.
 *
 * \note The GLOCK must be held on platforms which require the GLOCK
 *       for osi_FreeSmallSpace().
 *
 * \param[in]  vattr  pointer to the vattr to free; may be NULL
 */
void
afs_DestroyAttr(struct vattr *vattr)
{
    if (vattr) {
	osi_FreeSmallSpace(vattr);
    }
}

