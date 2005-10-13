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
 * afs_ustrategy
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#if !defined(AFS_HPUX_ENV) && !defined(AFS_SGI_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN80_ENV)

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"



int
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
afs_ustrategy(abp, credp)
     struct AFS_UCRED *credp;
#else
afs_ustrategy(abp)
#endif
     register struct buf *abp;
{
    register afs_int32 code;
    struct uio tuio;
    struct uio *tuiop = &tuio;
    struct iovec tiovec[1];
    register struct vcache *tvc = VTOAFS(abp->b_vp);
    register afs_int32 len = abp->b_bcount;
#if	!defined(AFS_SUN5_ENV) && !defined(AFS_OSF_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
#ifdef	AFS_AIX41_ENV
    struct ucred *credp;
#else
    struct AFS_UCRED *credp = u.u_cred;
#endif
#endif

    AFS_STATCNT(afs_ustrategy);
#ifdef	AFS_AIX41_ENV
    /*
     * So that it won't change while reading it
     */
    ObtainReadLock(&tvc->lock);
    if (tvc->credp) {
	credp = tvc->credp;
	crhold(credp);
    } else {
	credp = crref();
    }
    ReleaseReadLock(&tvc->lock);
    osi_Assert(credp);
#endif
#ifdef AFS_FBSD50_ENV
    if (abp->b_iocmd == BIO_READ) {
#else
    if ((abp->b_flags & B_READ) == B_READ) {
#endif
	/* read b_bcount bytes into kernel address b_un.b_addr starting
	 * at byte DEV_BSIZE * b_blkno.  Bzero anything we can't read,
	 * and finally call iodone(abp).  File is in abp->b_vp.  Credentials
	 * are from u area??
	 */
	tuio.afsio_iov = tiovec;
	tuio.afsio_iovcnt = 1;
#if	defined(AFS_OSF_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_XBSD_ENV)
#ifdef AFS_64BIT_CLIENT
#ifdef AFS_SUN5_ENV
	tuio.afsio_offset = (afs_offs_t) ldbtob(abp->b_lblkno);
#else
	tuio.afsio_offset = (afs_offs_t) dbtob(abp->b_blkno);
#endif
#else /* AFS_64BIT_CLIENT */
	tuio.afsio_offset = (u_int) dbtob(abp->b_blkno);
#endif /* AFS_64BIT_CLIENT */
#else
	tuio.afsio_offset = DEV_BSIZE * abp->b_blkno;
#endif
	tuio.afsio_seg = AFS_UIOSYS;
#ifdef AFS_UIOFMODE
	tuio.afsio_fmode = 0;
#endif
	tuio.afsio_resid = abp->b_bcount;
#if defined(AFS_XBSD_ENV)
	tiovec[0].iov_base = abp->b_saveaddr;
#else
	tiovec[0].iov_base = abp->b_un.b_addr;
#endif /* AFS_XBSD_ENV */
	tiovec[0].iov_len = abp->b_bcount;
	/* are user's credentials valid here?  probably, but this
	 * sure seems like the wrong things to do. */
#if	defined(AFS_SUN5_ENV)
	code = afs_nlrdwr(VTOAFS(abp->b_vp), &tuio, UIO_READ, 0, credp);
#else
	code = afs_rdwr(VTOAFS(abp->b_vp), &tuio, UIO_READ, 0, credp);
#endif
	if (code == 0) {
	    if (tuio.afsio_resid > 0)
#if defined(AFS_XBSD_ENV)
		memset(abp->b_saveaddr + abp->b_bcount - tuio.afsio_resid, 0,
		       tuio.afsio_resid);
#else
		memset(abp->b_un.b_addr + abp->b_bcount - tuio.afsio_resid, 0,
		       tuio.afsio_resid);
#endif /* AFS_XBSD_ENV */
#ifdef	AFS_AIX32_ENV
	    /*
	     * If we read a block that is past EOF and the user was not storing
	     * to it, go ahead and write protect the page. This way we will detect
	     * storing beyond EOF in the future
	     */
	    if (dbtob(abp->b_blkno) + abp->b_bcount > tvc->m.Length) {
		if ((abp->b_flags & B_PFSTORE) == 0) {
		    AFS_GUNLOCK();
		    vm_protectp(tvc->segid, dbtob(abp->b_blkno) / PAGESIZE,
				abp->b_bcount / PAGESIZE, RDONLY);
		    AFS_GLOCK();
		}
	    }
#endif
	}
    } else {
	tuio.afsio_iov = tiovec;
	tuio.afsio_iovcnt = 1;
#if	defined(AFS_OSF_ENV) || defined(AFS_SUN5_ENV)
#ifdef AFS_64BIT_CLIENT
#ifdef AFS_SUN5_ENV
	tuio.afsio_offset = (afs_offs_t) ldbtob(abp->b_lblkno);
#else
	tuio.afsio_offset = (afs_offs_t) dbtob(abp->b_blkno);
#endif
#else /* AFS_64BIT_CLIENT */
	tuio.afsio_offset = (u_int) dbtob(abp->b_blkno);
#endif /* AFS_64BIT_CLIENT */
#ifdef	AFS_SUN5_ENV
#ifdef	AFS_SUN59_ENV
	tuio.uio_limit = curproc->p_fsz_ctl.rlim_cur;
#else
	tuio.uio_limit = u.u_rlimit[RLIMIT_FSIZE].rlim_cur;
#endif
#endif
#else
	tuio.afsio_offset = DEV_BSIZE * abp->b_blkno;
#endif
	tuio.afsio_seg = AFS_UIOSYS;
#ifdef AFS_UIOFMODE
	tuio.afsio_fmode = 0;
#endif
#ifdef	AFS_AIX32_ENV
	/*
	 * XXX It this really right? Ideally we should always write block size multiple
	 * and not any arbitrary size, right? XXX
	 */
	len = MIN(len, tvc->m.Length - dbtob(abp->b_blkno));
#endif
#ifdef AFS_OSF_ENV
	len =
	    MIN(abp->b_bcount,
		(VTOAFS(abp->b_vp))->m.Length - dbtob(abp->b_blkno));
#endif /* AFS_OSF_ENV */
	tuio.afsio_resid = len;
#if defined(AFS_XBSD_ENV)
	tiovec[0].iov_base = abp->b_saveaddr;
#else
	tiovec[0].iov_base = abp->b_un.b_addr;
#endif /* AFS_XBSD_ENV */
	tiovec[0].iov_len = len;
	/* are user's credentials valid here?  probably, but this
	 * sure seems like the wrong things to do. */
#if	defined(AFS_SUN5_ENV)
	code = afs_nlrdwr(VTOAFS(abp->b_vp), &tuio, UIO_WRITE, 0, credp);
#else
	code = afs_rdwr(VTOAFS(abp->b_vp), &tuio, UIO_WRITE, 0, credp);
#endif
    }

#if defined(AFS_DUX40_ENV) || defined (AFS_XBSD_ENV)
    if (code) {
	abp->b_error = code;
#if !defined(AFS_FBSD50_ENV)
	abp->b_flags |= B_ERROR;
#endif
    }
#endif

#if defined(AFS_AIX32_ENV)
    crfree(credp);
#elif defined(AFS_DUX40_ENV)
    biodone(abp);
    if (code && !(abp->b_flags & B_READ)) {
	/* prevent ubc from retrying writes */
	AFS_GUNLOCK();
	ubc_invalidate(AFSTOV(tvc)->v_object,
		       (vm_offset_t) dbtob(abp->b_blkno), PAGE_SIZE, B_INVAL);
	AFS_GLOCK();
    }
#elif defined(AFS_FBSD60_ENV)
    (*abp->b_iodone)(abp);
#elif defined(AFS_FBSD50_ENV)
    biodone(&abp->b_io);
#elif defined(AFS_XBSD_ENV)
    biodone(abp);
#elif !defined(AFS_SUN5_ENV)
    iodone(abp);
#endif

    afs_Trace3(afs_iclSetp, CM_TRACE_STRATEGYDONE, ICL_TYPE_POINTER, tvc,
	       ICL_TYPE_INT32, code, ICL_TYPE_LONG, tuio.afsio_resid);
    return code;
}

#endif /* !AFS_HPUX_ENV  && !AFS_SGI_ENV && !AFS_LINUX20_ENV */
