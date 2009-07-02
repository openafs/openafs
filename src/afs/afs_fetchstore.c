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

extern int cacheDiskType;

/* rock and operations for RX_FILESERVER */

struct rxfs_storeVariables {
    struct rx_call *call;
    char *tbuffer;
    struct iovec *tiov;
    afs_uint32 tnio;
    afs_int32 hasNo64bit;
    struct AFSStoreStatus InStatus;
};

afs_int32
rxfs_storeUfsPrepare(void *r, afs_uint32 size, afs_uint32 *tlen)
{
    *tlen = (size > AFS_LRALLOCSIZ ?  AFS_LRALLOCSIZ : size);
    return 0;
}

afs_int32
rxfs_storeMemPrepare(void *r, afs_uint32 size, afs_uint32 *tlen)
{
    afs_int32 code;
    struct rxfs_storeVariables *v = (struct rxfs_storeVariables *) r;

    *tlen = (size > AFS_LRALLOCSIZ ?  AFS_LRALLOCSIZ : size);
    RX_AFS_GUNLOCK();
    code = rx_WritevAlloc(v->call, v->tiov, &v->tnio, RX_MAXIOVECS, *tlen);
    RX_AFS_GLOCK();
    if (code <= 0) {
	code = rx_Error(v->call);
	if ( !code )
	    code = -33;
    }
    else {
	*tlen = code;
	code = 0;
    }
    return code;
}

afs_int32
rxfs_storeUfsRead(void *r, struct osi_file *tfile, afs_uint32 offset,
		  afs_uint32 tlen, afs_uint32 *got)
{
    afs_int32 code;
    struct rxfs_storeVariables *v = (struct rxfs_storeVariables *)r;

    *got = afs_osi_Read(tfile, -1, v->tbuffer, tlen);
    if ((*got < 0)
#if defined(KERNEL_HAVE_UERROR)
		|| (*got != tlen && getuerror())
#endif
		)
	return EIO;
    return 0;
}

afs_int32
rxfs_storeMemRead(void *r, struct osi_file *tfile, afs_uint32 offset,
		  afs_uint32 tlen, afs_uint32 *bytesread)
{
    afs_int32 code;
    struct rxfs_storeVariables *v = (struct rxfs_storeVariables *)r;
    struct memCacheEntry *mceP = (struct memCacheEntry *)tfile;

    *bytesread = 0;
    code = afs_MemReadvBlk(mceP, offset, v->tiov, v->tnio, tlen);
    if (code != tlen)
	return -33;
    *bytesread = code;
    return 0;
}

afs_int32
rxfs_storeMemWrite(void *r, afs_uint32 l, afs_uint32 *byteswritten)
{
    afs_int32 code;
    struct rxfs_storeVariables *v = (struct rxfs_storeVariables *)r;

    RX_AFS_GUNLOCK();
    code = rx_Writev(v->call, v->tiov, v->tnio, l);
    RX_AFS_GLOCK();
    if (code != l) {
	code = rx_Error(v->call);
        return (code ? code : -33);
    }
    *byteswritten = code;
    return 0;
}

afs_int32
rxfs_storeUfsWrite(void *r, afs_uint32 l, afs_uint32 *byteswritten)
{
    afs_int32 code;
    struct rxfs_storeVariables *v = (struct rxfs_storeVariables *)r;

    RX_AFS_GUNLOCK();
    code = rx_Write(v->call, v->tbuffer, l);
	/* writing 0 bytes will
	 * push a short packet.  Is that really what we want, just because the
	 * data didn't come back from the disk yet?  Let's try it and see. */
    RX_AFS_GLOCK();
    if (code != l) {
	code = rx_Error(v->call);
        return (code ? code : -33);
    }
    *byteswritten = code;
    return 0;
}

afs_int32
rxfs_storeStatus(void *rock)
{
    struct rxfs_storeVariables *v = (struct rxfs_storeVariables *)rock;

    if (rx_GetRemoteStatus(v->call) & 1)
	return 0;
    return 1;
}

afs_int32
rxfs_storeDestroy(void **r, afs_int32 error)
{
    afs_int32 code = error;
    struct rxfs_storeVariables *v = (struct rxfs_storeVariables *)*r;

    *r = NULL;
    if (v->tbuffer)
	osi_FreeLargeSpace(v->tbuffer);
    if (v->tiov)
	osi_FreeSmallSpace(v->tiov);
    osi_FreeSmallSpace(v);
    return code;
}

static
struct storeOps rxfs_storeUfsOps = {
    rxfs_storeUfsPrepare,
    rxfs_storeUfsRead,
    rxfs_storeUfsWrite,
    rxfs_storeStatus,
    rxfs_storeDestroy
};

static
struct storeOps rxfs_storeMemOps = {
    rxfs_storeMemPrepare,
    rxfs_storeMemRead,
    rxfs_storeMemWrite,
    rxfs_storeStatus,
    rxfs_storeDestroy
};

afs_int32
rxfs_storeInit(struct vcache *avc, struct storeOps **ops, void **rock)
{
    struct rxfs_storeVariables *v;

    v = (struct rxfs_storeVariables *) osi_AllocSmallSpace(sizeof(struct rxfs_storeVariables));
    if (!v)
        osi_Panic("rxfs_storeInit: osi_AllocSmallSpace returned NULL\n");
    memset(v, 0, sizeof(struct rxfs_storeVariables));

    v->InStatus.ClientModTime = avc->f.m.Date;
    v->InStatus.Mask = AFS_SETMODTIME;

    if (cacheDiskType == AFS_FCACHE_TYPE_UFS) {
	v->tbuffer = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
	if (!v->tbuffer)
	    osi_Panic
              ("rxfs_storeInit: osi_AllocLargeSpace for iovecs returned NULL\n");
	*ops = (struct storeOps *) &rxfs_storeUfsOps;
    } else {
	v->tiov = osi_AllocSmallSpace(sizeof(struct iovec) * RX_MAXIOVECS);
	if (!v->tiov)
	    osi_Panic
              ("rxfs_storeInit: osi_AllocSmallSpace for iovecs returned NULL\n");
	*ops = (struct storeOps *) &rxfs_storeMemOps;
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
    }
    *rock = (void *)v;
    return 0;
}

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

/*!
 *	Called upon store.
 *
 * \param acall Ptr to the Rx call structure involved.
 * \param fP Ptr to the related file descriptor.
 * \param alen Size of the file in bytes.
 * \param avc Ptr to the vcache entry.
 * \param shouldWake is it "safe" to return early from close() ?
 * \param abytesToXferP Set to the number of bytes to xfer.
 *	NOTE: This parameter is only used if AFS_NOSTATS is not defined.
 * \param abytesXferredP Set to the number of bytes actually xferred.
 *	NOTE: This parameter is only used if AFS_NOSTATS is not defined.
 *
 * \note Environment: Nothing interesting.
 */
int
afs_CacheStoreProc(register struct rx_call *acall,
		      register struct osi_file *fP,
		      register afs_int32 alen, struct vcache *avc,
		      int *shouldWake, afs_size_t * abytesToXferP,
		      afs_size_t * abytesXferredP)
{
    afs_int32 code;
    afs_uint32 tlen;
    int offset = 0;
    struct storeOps *ops;
    void * rock = NULL;

    afs_Trace4(afs_iclSetp, CM_TRACE_STOREPROC, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_FID, &(avc->f.fid), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->f.m.Length), ICL_TYPE_INT32, alen);
    code =  rxfs_storeInit(avc, &ops, &rock);
    if ( code ) {
	osi_Panic("afs_CacheStoreProc: rxfs_storeInit failed");
    }
    ((struct rxfs_storeVariables *)rock)->call = acall;

    AFS_STATCNT(CacheStoreProc);
#ifndef AFS_NOSTATS
    /*
     * In this case, alen is *always* the amount of data we'll be trying
     * to ship here.
     */
    *(abytesToXferP) = alen;
    *(abytesXferredP) = 0;
#endif /* AFS_NOSTATS */

    while ( alen > 0 ) {
	afs_int32 bytesread, byteswritten;
	code = (*ops->prepare)(rock, alen, &tlen);
	if ( code )
	    break;

	code = (*ops->read)(rock, fP, offset, tlen, &bytesread);
	if (code)
	    break;

	tlen = bytesread;
	code = (*ops->write)(rock, tlen, &byteswritten);
	if (code)
	    break;
#ifndef AFS_NOSTATS
	(*abytesXferredP) += byteswritten;
#endif /* AFS_NOSTATS */

	offset += tlen;
	alen -= tlen;
	/*
	 * if file has been locked on server, can allow
	 * store to continue
	 */
	if (shouldWake && *shouldWake && ((*ops->status)(rock) == 0)) {
	    *shouldWake = 0;	/* only do this once */
	    afs_wakeup(avc);
	}
    }
    afs_Trace4(afs_iclSetp, CM_TRACE_STOREPROC, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_FID, &(avc->f.fid), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->f.m.Length), ICL_TYPE_INT32, alen);
    code = (*ops->destroy)(&rock, code);
    return code;
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

