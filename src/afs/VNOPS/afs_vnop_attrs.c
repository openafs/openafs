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
 *
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/VNOPS/afs_vnop_attrs.c,v 1.27.2.11 2006/11/10 00:08:57 shadow Exp $");

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
afs_CopyOutAttrs(register struct vcache *avc, register struct vattr *attrs)
{
    register struct volume *tvp;
    register struct cell *tcell;
    int fakedir = 0;

    AFS_STATCNT(afs_CopyOutAttrs);
    if (afs_fakestat_enable && avc->mvstat == 1)
	fakedir = 1;
    attrs->va_type = fakedir ? VDIR : vType(avc);
#if defined(AFS_SGI_ENV) || defined(AFS_AIX32_ENV) || defined(AFS_SUN5_ENV)
    attrs->va_mode = fakedir ? 0755 : (mode_t) (avc->m.Mode & 0xffff);
#else
    attrs->va_mode = fakedir ? VDIR | 0755 : avc->m.Mode;
#endif

    if (avc->m.Mode & (VSUID | VSGID)) {
	/* setuid or setgid, make sure we're allowed to run them from this cell */
	tcell = afs_GetCell(avc->fid.Cell, 0);
	if (tcell && (tcell->states & CNoSUID))
	    attrs->va_mode &= ~(VSUID | VSGID);
    }
#if defined(AFS_DARWIN_ENV)
    {
	extern u_int32_t afs_darwin_realmodes;
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
    attrs->va_uid = fakedir ? 0 : avc->m.Owner;
    attrs->va_gid = fakedir ? 0 : avc->m.Group;	/* yeah! */
#if defined(AFS_SUN56_ENV)
    attrs->va_fsid = avc->v.v_vfsp->vfs_fsid.val[0];
#elif defined(AFS_OSF_ENV)
    attrs->va_fsid = avc->v.v_mount->m_stat.f_fsid.val[0];
#elif defined(AFS_DARWIN80_ENV)
    VATTR_RETURN(attrs, va_fsid, vfs_statfs(vnode_mount(AFSTOV(avc)))->f_fsid.val[0]);
#elif defined(AFS_DARWIN70_ENV)
    attrs->va_fsid = avc->v->v_mount->mnt_stat.f_fsid.val[0];
#else /* ! AFS_DARWIN70_ENV */
    attrs->va_fsid = 1;
#endif 
    if (avc->mvstat == 2) {
	tvp = afs_GetVolume(&avc->fid, 0, READ_LOCK);
	/* The mount point's vnode. */
	if (tvp) {
	    attrs->va_nodeid =
		tvp->mtpoint.Fid.Vnode + (tvp->mtpoint.Fid.Volume << 16);
	    if (FidCmp(&afs_rootFid, &avc->fid) && !attrs->va_nodeid)
		attrs->va_nodeid = 2;
	    afs_PutVolume(tvp, READ_LOCK);
	} else
	    attrs->va_nodeid = 2;
    } else
	attrs->va_nodeid = avc->fid.Fid.Vnode + (avc->fid.Fid.Volume << 16);
    attrs->va_nodeid &= 0x7fffffff;	/* Saber C hates negative inode #s! */
    attrs->va_nlink = fakedir ? 100 : avc->m.LinkCount;
    attrs->va_size = fakedir ? 4096 : avc->m.Length;
    attrs->va_atime.tv_sec = attrs->va_mtime.tv_sec = attrs->va_ctime.tv_sec =
	fakedir ? 0 : (int)avc->m.Date;
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
    attrs->va_gen = hgetlo(avc->m.DataVersion);
#elif defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV) || defined(AFS_OBSD_ENV)
    attrs->va_atime.tv_nsec = attrs->va_mtime.tv_nsec =
	attrs->va_ctime.tv_nsec =
	(hgetlo(avc->m.DataVersion) & 0x7ffff) * 1000;
#else
    attrs->va_atime.tv_usec = attrs->va_mtime.tv_usec =
	attrs->va_ctime.tv_usec = (hgetlo(avc->m.DataVersion) & 0x7ffff);
#endif
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV) || defined(AFS_OSF_ENV)
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
#if defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
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
	    struct AFS_UCRED *acred)
#else
int
afs_getattr(OSI_VC_DECL(avc), struct vattr *attrs, struct AFS_UCRED *acred)
#endif
{
    afs_int32 code;
    struct vrequest treq;
    extern struct unixuser *afs_FindUser();
    struct unixuser *au;
    int inited = 0;
    OSI_VC_CONVERT(avc);

    AFS_STATCNT(afs_getattr);
    afs_Trace2(afs_iclSetp, CM_TRACE_GETATTR, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->m.Length));

    if (afs_fakestat_enable && avc->mvstat == 1) {
	struct afs_fakestat_state fakestat;

	code = afs_InitReq(&treq, acred);
	if (code)
	    return code;
	afs_InitFakeStat(&fakestat);
	code = afs_TryEvalFakeStat(&avc, &fakestat, &treq);
	if (code) {
	    afs_PutFakeStat(&fakestat);
	    return code;
	}

	code = afs_CopyOutAttrs(avc, attrs);
	afs_PutFakeStat(&fakestat);
	return code;
    }
#if defined(AFS_SUN5_ENV)
    if (flags & ATTR_HINT) {
	code = afs_CopyOutAttrs(avc, attrs);
	return code;
    }
#endif
#if defined(AFS_DARWIN_ENV) && !defined(AFS_DARWIN80_ENV)
    if (avc->states & CUBCinit) {
	code = afs_CopyOutAttrs(avc, attrs);
	return code;
    }
#endif

#ifdef AFS_BOZONLOCK_ENV
    afs_BozonLock(&avc->pvnLock, avc);
#endif

    if (afs_shuttingdown)
	return EIO;

    if (!(avc->states & CStatd)) {
	if (!(code = afs_InitReq(&treq, acred))) {
	    code = afs_VerifyVCache2(avc, &treq);
	    inited = 1;
	}
    } else
	code = 0;

#ifdef AFS_BOZONLOCK_ENV
    if (code == 0)
	osi_FlushPages(avc, acred);
    afs_BozonUnlock(&avc->pvnLock, avc);
#endif


    if (code == 0) {
	osi_FlushText(avc);	/* only needed to flush text if text locked last time */
	code = afs_CopyOutAttrs(avc, attrs);

	if (afs_nfsexporter) {
	    if (!inited) {
		if ((code = afs_InitReq(&treq, acred)))
		    return code;
		inited = 1;
	    }
	    if (AFS_NFSXLATORREQ(acred)) {
		if ((vType(avc) != VDIR)
		    && !afs_AccessOK(avc, PRSFS_READ, &treq,
				     CHECK_MODE_BITS |
				     CMB_ALLOW_EXEC_AS_READ)) {
		    return EACCES;
		}
	    }
	    if ((au = afs_FindUser(treq.uid, -1, READ_LOCK))) {
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
			struct inode *ip = AFSTOV(avc)->i_sb->s_root->d_inode;
			attrs->va_nodeid = ip->i_ino;	/* VTOI()? */
		    }
#else
		    if (
#ifdef AFS_DARWIN_ENV		    
			vnode_isvroot(AFSTOV(avc))
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
    if (!code)
	return 0;
    code = afs_CheckCode(code, &treq, 14);
    return code;
}

/* convert a Unix request into a status store request */
int
afs_VAttrToAS(register struct vcache *avc, register struct vattr *av,
	      register struct AFSStoreStatus *as)
{
    register int mask;
    mask = 0;
    AFS_STATCNT(afs_VAttrToAS);
#if     defined(AFS_DARWIN80_ENV)
    if (VATTR_IS_ACTIVE(av, va_mode)) {
#elif	defined(AFS_AIX_ENV)
/* Boy, was this machine dependent bogosity hard to swallow????.... */
    if (av->va_mode != -1) {
#elif	defined(AFS_LINUX22_ENV)
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
	if (avc->states & CForeign) {
	    ObtainWriteLock(&avc->lock, 127);
	    afs_FreeAllAxs(&(avc->Access));
	    ReleaseWriteLock(&avc->lock);
	}
    }
#if     defined(AFS_DARWIN80_ENV)
    if (VATTR_IS_ACTIVE(av, va_gid)) {
#elif defined(AFS_LINUX22_ENV)
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
#elif defined(AFS_LINUX22_ENV)
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
#elif	defined(AFS_LINUX22_ENV)
    if (av->va_mask & ATTR_MTIME) {
#elif	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (av->va_mask & AT_MTIME) {
#else
    if (av->va_mtime.tv_sec != -1) {
#endif
	mask |= AFS_SETMODTIME;
#ifndef	AFS_SGI_ENV
#if	defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
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
#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
int
afs_setattr(OSI_VC_DECL(avc), register struct vattr *attrs, int flags,
	    struct AFS_UCRED *acred)
#else
int
afs_setattr(OSI_VC_DECL(avc), register struct vattr *attrs,
	    struct AFS_UCRED *acred)
#endif
{
    struct vrequest treq;
    struct AFSStoreStatus astat;
    register afs_int32 code;
    struct afs_fakestat_state fakestate;
    OSI_VC_CONVERT(avc);

    AFS_STATCNT(afs_setattr);
#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX22_ENV)
    afs_Trace4(afs_iclSetp, CM_TRACE_SETATTR, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, attrs->va_mask, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(attrs->va_size), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->m.Length));
#else
    afs_Trace4(afs_iclSetp, CM_TRACE_SETATTR, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, attrs->va_mode, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(attrs->va_size), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->m.Length));
#endif
    if ((code = afs_InitReq(&treq, acred)))
	return code;

    afs_InitFakeStat(&fakestate);
    code = afs_EvalFakeStat(&avc, &fakestate, &treq);
    if (code)
	goto done;

    if (avc->states & CRO) {
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
#elif	defined(AFS_LINUX22_ENV)
    if (attrs->va_mask & ATTR_SIZE) {
#elif	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (attrs->va_mask & AT_SIZE) {
#elif	defined(AFS_OSF_ENV) || defined(AFS_XBSD_ENV)
    if (attrs->va_size != VNOVAL) {
#elif	defined(AFS_AIX41_ENV)
    if (attrs->va_size != -1) {
#else
    if (attrs->va_size != ~0) {
#endif
	if (!afs_AccessOK(avc, PRSFS_WRITE, &treq, DONT_CHECK_MODE_BITS)) {
	    code = EACCES;
	    goto done;
	}
    }

    afs_VAttrToAS(avc, attrs, &astat);	/* interpret request */
    code = 0;
#ifdef AFS_BOZONLOCK_ENV
    afs_BozonLock(&avc->pvnLock, avc);
#endif
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
#elif	defined(AFS_LINUX22_ENV)
    if (attrs->va_mask & ATTR_SIZE) {
#elif	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
    if (attrs->va_mask & AT_SIZE) {
#elif	defined(AFS_OSF_ENV) || defined(AFS_XBSD_ENV)
    if (attrs->va_size != VNOVAL) {
#else
    if (attrs->va_size != -1) {
#endif
	afs_size_t tsize = attrs->va_size;
	ObtainWriteLock(&avc->lock, 128);
	avc->states |= CDirty;
	code = afs_TruncateAllSegments(avc, tsize, &treq, acred);
	/* if date not explicitly set by this call, set it ourselves, since we
	 * changed the data */
	if (!(astat.Mask & AFS_SETMODTIME)) {
	    astat.Mask |= AFS_SETMODTIME;
	    astat.ClientModTime = osi_Time();
	}
	if (code == 0) {
	    if (((avc->execsOrWriters <= 0) && (avc->states & CCreating) == 0)
		|| (avc->execsOrWriters == 1 && AFS_NFSXLATORREQ(acred))) {
		code = afs_StoreAllSegments(avc, &treq, AFS_ASYNC);
		if (!code)
		    avc->states &= ~CDirty;
	    }
	} else
	    avc->states &= ~CDirty;

	ReleaseWriteLock(&avc->lock);
	hzero(avc->flushDV);
	osi_FlushText(avc);	/* do this after releasing all locks */
    }
    if (code == 0) {
	ObtainSharedLock(&avc->lock, 16);	/* lock entry */
	code = afs_WriteVCache(avc, &astat, &treq);	/* send request */
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
#ifdef AFS_BOZONLOCK_ENV
    afs_BozonUnlock(&avc->pvnLock, avc);
#endif
#if defined(AFS_SGI_ENV)
    AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#endif
  done:
    afs_PutFakeStat(&fakestate);
    code = afs_CheckCode(code, &treq, 15);
    return code;
}
