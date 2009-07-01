/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#ifndef AFS_LINUX22_ENV
#include "rpc/types.h"
#endif
#ifdef	AFS_ALPHA_ENV
#undef kmem_alloc
#undef kmem_free
#undef mem_alloc
#undef mem_free
#undef register
#endif /* AFS_ALPHA_ENV */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs_prototypes.h"

/*
 * afs_UFSCacheStoreProc
 *
 * Description:
 *	Called upon store.
 *
 * Parameters:
 *	acall : Ptr to the Rx call structure involved.
 *	afile : Ptr to the related file descriptor.
 *	alen  : Size of the file in bytes.
 *	avc   : Ptr to the vcache entry.
 *      shouldWake : is it "safe" to return early from close() ?
 *	abytesToXferP  : Set to the number of bytes to xfer.
 *			 NOTE: This parameter is only used if AFS_NOSTATS
 *				is not defined.
 *	abytesXferredP : Set to the number of bytes actually xferred.
 *			 NOTE: This parameter is only used if AFS_NOSTATS
 *				is not defined.
 *
 * Environment:
 *	Nothing interesting.
 */
int
afs_UFSCacheStoreProc(register struct rx_call *acall, struct osi_file *afile,
		      register afs_int32 alen, struct vcache *avc,
		      int *shouldWake, afs_size_t * abytesToXferP,
		      afs_size_t * abytesXferredP)
{
    afs_int32 code, got;
    register char *tbuffer;
    register int tlen;

    AFS_STATCNT(UFS_CacheStoreProc);

#ifndef AFS_NOSTATS
    /*
     * In this case, alen is *always* the amount of data we'll be trying
     * to ship here.
     */
    (*abytesToXferP) = alen;
    (*abytesXferredP) = 0;
#endif /* AFS_NOSTATS */

    afs_Trace4(afs_iclSetp, CM_TRACE_STOREPROC, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_FID, &(avc->f.fid), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->f.m.Length), ICL_TYPE_INT32, alen);
    tbuffer = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    while (alen > 0) {
	tlen = (alen > AFS_LRALLOCSIZ ? AFS_LRALLOCSIZ : alen);
	got = afs_osi_Read(afile, -1, tbuffer, tlen);
	if ((got < 0)
#if defined(KERNEL_HAVE_UERROR)
	    || (got != tlen && getuerror())
#endif
	    ) {
	    osi_FreeLargeSpace(tbuffer);
	    return EIO;
	}
	afs_Trace2(afs_iclSetp, CM_TRACE_STOREPROC2, ICL_TYPE_OFFSET,
		   ICL_HANDLE_OFFSET(*tbuffer), ICL_TYPE_INT32, got);
	RX_AFS_GUNLOCK();
	code = rx_Write(acall, tbuffer, got);	/* writing 0 bytes will
						 * push a short packet.  Is that really what we want, just because the
						 * data didn't come back from the disk yet?  Let's try it and see. */
	RX_AFS_GLOCK();
#ifndef AFS_NOSTATS
	(*abytesXferredP) += code;
#endif /* AFS_NOSTATS */
	if (code != got) {
	    code = rx_Error(acall);
	    osi_FreeLargeSpace(tbuffer);
	    return code ? code : -33;
	}
	alen -= got;
	/*
	 * If file has been locked on server, we can allow the store
	 * to continue.
	 */
	if (shouldWake && *shouldWake && (rx_GetRemoteStatus(acall) & 1)) {
	    *shouldWake = 0;	/* only do this once */
	    afs_wakeup(avc);
	}
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_STOREPROC, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_FID, &(avc->f.fid), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->f.m.Length), ICL_TYPE_INT32, alen);
    osi_FreeLargeSpace(tbuffer);
    return 0;

}				/* afs_UFSCacheStoreProc */


/*
 * afs_UFSCacheFetchProc
 *
 * Description:
 *	Routine called on fetch; also tells people waiting for data
 *	that more has arrived.
 *
 * Parameters:
 *	acall : Ptr to the Rx call structure.
 *	afile : File descriptor for the cache file.
 *	abase : Base offset to fetch.
 *	adc   : Ptr to the dcache entry for the file, write-locked.
 *      avc   : Ptr to the vcache entry for the file.
 *	abytesToXferP  : Set to the number of bytes to xfer.
 *			 NOTE: This parameter is only used if AFS_NOSTATS
 *				is not defined.
 *	abytesXferredP : Set to the number of bytes actually xferred.
 *			 NOTE: This parameter is only used if AFS_NOSTATS
 *				is not defined.
 *
 * Environment:
 *	Nothing interesting.
 */

int
afs_UFSCacheFetchProc(register struct rx_call *acall, struct osi_file *afile,
		      afs_size_t abase, struct dcache *adc,
		      struct vcache *avc, afs_size_t * abytesToXferP,
		      afs_size_t * abytesXferredP, afs_int32 lengthFound)
{
    afs_int32 length;
    register afs_int32 code;
    register char *tbuffer;
    register int tlen;
    int moredata = 0;

    AFS_STATCNT(UFS_CacheFetchProc);
    osi_Assert(WriteLocked(&adc->lock));
    afile->offset = 0;		/* Each time start from the beginning */
    length = lengthFound;
#ifndef AFS_NOSTATS
    (*abytesToXferP) = 0;
    (*abytesXferredP) = 0;
#endif /* AFS_NOSTATS */
    tbuffer = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    adc->validPos = abase;
    do {
	if (moredata) {
	    RX_AFS_GUNLOCK();
	    code = rx_Read(acall, (char *)&length, sizeof(afs_int32));
	    RX_AFS_GLOCK();
	    length = ntohl(length);
	    if (code != sizeof(afs_int32)) {
		osi_FreeLargeSpace(tbuffer);
		code = rx_Error(acall);
		return (code ? code : -1);	/* try to return code, not -1 */
	    }
	}
	/*
	 * The fetch protocol is extended for the AFS/DFS translator
	 * to allow multiple blocks of data, each with its own length,
	 * to be returned. As long as the top bit is set, there are more
	 * blocks expected.
	 *
	 * We do not do this for AFS file servers because they sometimes
	 * return large negative numbers as the transfer size.
	 */
	if (avc->f.states & CForeign) {
	    moredata = length & 0x80000000;
	    length &= ~0x80000000;
	} else {
	    moredata = 0;
	}
#ifndef AFS_NOSTATS
	(*abytesToXferP) += length;
#endif /* AFS_NOSTATS */
	while (length > 0) {
	    tlen = (length > AFS_LRALLOCSIZ ? AFS_LRALLOCSIZ : length);
#ifdef RX_KERNEL_TRACE
	    afs_Trace1(afs_iclSetp, CM_TRACE_TIMESTAMP, ICL_TYPE_STRING,
		       "before rx_Read");
#endif
	    RX_AFS_GUNLOCK();
	    code = rx_Read(acall, tbuffer, tlen);
	    RX_AFS_GLOCK();
#ifdef RX_KERNEL_TRACE
	    afs_Trace1(afs_iclSetp, CM_TRACE_TIMESTAMP, ICL_TYPE_STRING,
		       "after rx_Read");
#endif
#ifndef AFS_NOSTATS
	    (*abytesXferredP) += code;
#endif /* AFS_NOSTATS */
	    if (code != tlen) {
		osi_FreeLargeSpace(tbuffer);
		afs_Trace3(afs_iclSetp, CM_TRACE_FETCH64READ,
			   ICL_TYPE_POINTER, avc, ICL_TYPE_INT32, code,
			   ICL_TYPE_INT32, length);
		return -34;
	    }
	    code = afs_osi_Write(afile, -1, tbuffer, tlen);
	    if (code != tlen) {
		osi_FreeLargeSpace(tbuffer);
		return EIO;
	    }
	    abase += tlen;
	    length -= tlen;
	    adc->validPos = abase;
	    if (afs_osi_Wakeup(&adc->validPos) == 0)
		afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAKE, ICL_TYPE_STRING,
			   __FILE__, ICL_TYPE_INT32, __LINE__,
			   ICL_TYPE_POINTER, adc, ICL_TYPE_INT32,
			   adc->dflags);
	}
    } while (moredata);
    osi_FreeLargeSpace(tbuffer);
    return 0;

}				/* afs_UFSCacheFetchProc */

int
afs_MemCacheStoreProc(register struct rx_call *acall,
		      register struct osi_file *fP,
		      register afs_int32 alen, struct vcache *avc,
		      int *shouldWake, afs_size_t * abytesToXferP,
		      afs_size_t * abytesXferredP)
{
    register struct memCacheEntry *mceP = (struct memCacheEntry *)fP;

    register afs_int32 code;
    register int tlen;
    int offset = 0;
    struct iovec *tiov;		/* no data copying with iovec */
    int tnio;			/* temp for iovec size */

    AFS_STATCNT(afs_MemCacheStoreProc);
#ifndef AFS_NOSTATS
    /*
     * In this case, alen is *always* the amount of data we'll be trying
     * to ship here.
     */
    *(abytesToXferP) = alen;
    *(abytesXferredP) = 0;
#endif /* AFS_NOSTATS */

    /*
     * We need to alloc the iovecs on the heap so that they are "pinned" rather than
     * declare them on the stack - defect 11272
     */
    tiov =
	(struct iovec *)osi_AllocSmallSpace(sizeof(struct iovec) *
					    RX_MAXIOVECS);
    if (!tiov) {
	osi_Panic
	    ("afs_MemCacheStoreProc: osi_AllocSmallSpace for iovecs returned NULL\n");
    }
#ifdef notdef
    /* do this at a higher level now -- it's a parameter */
    /* for now, only do 'continue from close' code if file fits in one
     * chunk.  Could clearly do better: if only one modified chunk
     * then can still do this.  can do this on *last* modified chunk */
    tlen = avc->f.m.Length - 1;	/* byte position of last byte we'll store */
    if (shouldWake) {
	if (AFS_CHUNK(tlen) != 0)
	    *shouldWake = 0;
	else
	    *shouldWake = 1;
    }
#endif /* notdef */

    while (alen > 0) {
	tlen = (alen > AFS_LRALLOCSIZ ? AFS_LRALLOCSIZ : alen);
	RX_AFS_GUNLOCK();
	code = rx_WritevAlloc(acall, tiov, &tnio, RX_MAXIOVECS, tlen);
	RX_AFS_GLOCK();
	if (code <= 0) {
	    code = rx_Error(acall);
	    osi_FreeSmallSpace(tiov);
	    return code ? code : -33;
	}
	tlen = code;
	code = afs_MemReadvBlk(mceP, offset, tiov, tnio, tlen);
	if (code != tlen) {
	    osi_FreeSmallSpace(tiov);
	    return -33;
	}
	RX_AFS_GUNLOCK();
	code = rx_Writev(acall, tiov, tnio, tlen);
	RX_AFS_GLOCK();
#ifndef AFS_NOSTATS
	(*abytesXferredP) += code;
#endif /* AFS_NOSTATS */
	if (code != tlen) {
	    code = rx_Error(acall);
	    osi_FreeSmallSpace(tiov);
	    return code ? code : -33;
	}
	offset += tlen;
	alen -= tlen;
	/* if file has been locked on server, can allow store to continue */
	if (shouldWake && *shouldWake && (rx_GetRemoteStatus(acall) & 1)) {
	    *shouldWake = 0;	/* only do this once */
	    afs_wakeup(avc);
	}
    }
    osi_FreeSmallSpace(tiov);
    return 0;
}

int
afs_MemCacheFetchProc(register struct rx_call *acall,
		      register struct osi_file *fP, afs_size_t abase,
		      struct dcache *adc, struct vcache *avc,
		      afs_size_t * abytesToXferP, afs_size_t * abytesXferredP,
		      afs_int32 lengthFound)
{
    register struct memCacheEntry *mceP = (struct memCacheEntry *)fP;
    register afs_int32 code;
    afs_int32 length;
    int moredata = 0;
    struct iovec *tiov;		/* no data copying with iovec */
    register int tlen, offset = 0;
    int tnio;			/* temp for iovec size */

    AFS_STATCNT(afs_MemCacheFetchProc);
    length = lengthFound;
    afs_Trace4(afs_iclSetp, CM_TRACE_MEMFETCH, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_POINTER, mceP, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(abase), ICL_TYPE_INT32, length);
#ifndef AFS_NOSTATS
    (*abytesToXferP) = 0;
    (*abytesXferredP) = 0;
#endif /* AFS_NOSTATS */
    /*
     * We need to alloc the iovecs on the heap so that they are "pinned" rather than
     * declare them on the stack - defect 11272
     */
    tiov =
	(struct iovec *)osi_AllocSmallSpace(sizeof(struct iovec) *
					    RX_MAXIOVECS);
    if (!tiov) {
	osi_Panic
	    ("afs_MemCacheFetchProc: osi_AllocSmallSpace for iovecs returned NULL\n");
    }
    adc->validPos = abase;
    do {
	if (moredata) {
	    RX_AFS_GUNLOCK();
	    code = rx_Read(acall, (char *)&length, sizeof(afs_int32));
	    length = ntohl(length);
	    RX_AFS_GLOCK();
	    if (code != sizeof(afs_int32)) {
		code = rx_Error(acall);
		osi_FreeSmallSpace(tiov);
		return (code ? code : -1);	/* try to return code, not -1 */
	    }
	}
	/*
	 * The fetch protocol is extended for the AFS/DFS translator
	 * to allow multiple blocks of data, each with its own length,
	 * to be returned. As long as the top bit is set, there are more
	 * blocks expected.
	 *
	 * We do not do this for AFS file servers because they sometimes
	 * return large negative numbers as the transfer size.
	 */
	if (avc->f.states & CForeign) {
	    moredata = length & 0x80000000;
	    length &= ~0x80000000;
	} else {
	    moredata = 0;
	}
#ifndef AFS_NOSTATS
	(*abytesToXferP) += length;
#endif /* AFS_NOSTATS */
	while (length > 0) {
	    tlen = (length > AFS_LRALLOCSIZ ? AFS_LRALLOCSIZ : length);
	    RX_AFS_GUNLOCK();
	    code = rx_Readv(acall, tiov, &tnio, RX_MAXIOVECS, tlen);
	    RX_AFS_GLOCK();
#ifndef AFS_NOSTATS
	    (*abytesXferredP) += code;
#endif /* AFS_NOSTATS */
	    if (code <= 0) {
		afs_Trace3(afs_iclSetp, CM_TRACE_FETCH64READ,
			   ICL_TYPE_POINTER, avc, ICL_TYPE_INT32, code,
			   ICL_TYPE_INT32, length);
		osi_FreeSmallSpace(tiov);
		return -34;
	    }
	    tlen = code;
	    afs_MemWritevBlk(mceP, offset, tiov, tnio, tlen);
	    offset += tlen;
	    abase += tlen;
	    length -= tlen;
	    adc->validPos = abase;
	    if (afs_osi_Wakeup(&adc->validPos) == 0)
		afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAKE, ICL_TYPE_STRING,
			   __FILE__, ICL_TYPE_INT32, __LINE__,
			   ICL_TYPE_POINTER, adc, ICL_TYPE_INT32,
			   adc->dflags);
	}
    } while (moredata);
    /* max of two sizes */
    osi_FreeSmallSpace(tiov);
    return 0;
}

