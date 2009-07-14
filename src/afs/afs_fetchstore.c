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
		  afs_uint32 tlen, afs_uint32 *bytesread)
{
    afs_int32 code;
    struct rxfs_storeVariables *v = (struct rxfs_storeVariables *)r;

    *bytesread = 0;
    code = afs_osi_Read(tfile, -1, v->tbuffer, tlen);
    if (code < 0)
	return EIO;
    *bytesread = code;
    if (code == tlen)
        return 0;
#if defined(KERNEL_HAVE_UERROR)
    if (getuerror())
	return EIO;
#endif
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

/* rock and operations for RX_FILESERVER */

struct rxfs_fetchVariables {
    struct rx_call *call;
    char *tbuffer;
    struct iovec *iov;
    afs_uint32 nio;
    afs_int32 hasNo64bit;
    afs_int32 iovno;
    afs_int32 iovmax;
};

afs_int32
rxfs_fetchUfsRead(void *r, afs_uint32 size, afs_uint32 *bytesread)
{
    afs_int32 code;
    afs_uint32 tlen;
    struct rxfs_fetchVariables *v = (struct rxfs_fetchVariables *)r;

    *bytesread = 0;
    tlen = (size > AFS_LRALLOCSIZ ?  AFS_LRALLOCSIZ : size);
    RX_AFS_GUNLOCK();
    code = rx_Read(v->call, v->tbuffer, tlen);
    RX_AFS_GLOCK();
    if (code <= 0)
	return -34;
    *bytesread = code;
    return 0;
}

afs_int32
rxfs_fetchMemRead(void *r, afs_uint32 tlen, afs_uint32 *bytesread)
{
    afs_int32 code;
    struct rxfs_fetchVariables *v = (struct rxfs_fetchVariables *)r;

    *bytesread = 0;
    RX_AFS_GUNLOCK();
    code = rx_Readv(v->call, v->iov, &v->nio, RX_MAXIOVECS, tlen);
    RX_AFS_GLOCK();
    if (code <= 0)
	return -34;
    *bytesread = code;
    return 0;
}


afs_int32
rxfs_fetchMemWrite(void *r, struct osi_file *fP,
			afs_uint32 offset, afs_uint32 tlen,
			afs_uint32 *byteswritten)
{
    afs_int32 code;
    struct rxfs_fetchVariables *v = (struct rxfs_fetchVariables *)r;
    struct memCacheEntry *mceP = (struct memCacheEntry *)fP;

    code = afs_MemWritevBlk(mceP, offset, v->iov, v->nio, tlen);
    if (code != tlen) {
        return EIO;
    }
    *byteswritten = code;
    return 0;
}

afs_int32
rxfs_fetchUfsWrite(void *r, struct osi_file *fP,
			afs_uint32 offset, afs_uint32 tlen,
			afs_uint32 *byteswritten)
{
    afs_int32 code;
    struct rxfs_fetchVariables *v = (struct rxfs_fetchVariables *)r;

    code = afs_osi_Write(fP, -1, v->tbuffer, tlen);
    if (code != tlen) {
        return EIO;
    }
    *byteswritten = code;
    return 0;
}


afs_int32
rxfs_fetchClose(void *r, struct vcache *avc, struct dcache * adc,
					struct afs_FetchOutput *tsmall)
{
    afs_int32 code, code1 = 0;
    struct rxfs_fetchVariables *v = (struct rxfs_fetchVariables *)r;

    if (!v->call)
	return -1;

    RX_AFS_GUNLOCK();
    code = EndRXAFS_FetchData(v->call, &tsmall->OutStatus,
			      &tsmall->CallBack,
			      &tsmall->tsync);
    RX_AFS_GLOCK();

    RX_AFS_GUNLOCK();
    if (v->call)
	code1 = rx_EndCall(v->call, code);
    RX_AFS_GLOCK();
    if (!code && code1)
	code = code1;

    v->call = NULL;

    return code;
}

afs_int32
rxfs_fetchDestroy(void **r, afs_int32 error)
{
    afs_int32 code = error;
    struct rxfs_fetchVariables *v = (struct rxfs_fetchVariables *)*r;

    *r = NULL;
    if (v->tbuffer)
	osi_FreeLargeSpace(v->tbuffer);
    if (v->iov)
	osi_FreeSmallSpace(v->iov);
    osi_FreeSmallSpace(v);
    return code;
}

afs_int32
rxfs_fetchMore(void *r, afs_uint32 *length, afs_uint32 *moredata)
{
    afs_int32 code;
    register struct rxfs_fetchVariables *v
	    = (struct rxfs_fetchVariables *)r;

    RX_AFS_GUNLOCK();
    code = rx_Read(v->call, (void *)length, sizeof(afs_int32));
    *length = ntohl(*length);
    RX_AFS_GLOCK();
    if (code != sizeof(afs_int32)) {
	code = rx_Error(v->call);
	return (code ? code : -1);	/* try to return code, not -1 */
    }
    return 0;
}

static
struct fetchOps rxfs_fetchUfsOps = {
    rxfs_fetchMore,
    rxfs_fetchUfsRead,
    rxfs_fetchUfsWrite,
    rxfs_fetchClose,
    rxfs_fetchDestroy
};

static
struct fetchOps rxfs_fetchMemOps = {
    rxfs_fetchMore,
    rxfs_fetchMemRead,
    rxfs_fetchMemWrite,
    rxfs_fetchClose,
    rxfs_fetchDestroy
};

afs_int32
rxfs_fetchInit(register struct afs_conn *tc, struct vcache *avc,afs_offs_t Position,
		afs_uint32 size, afs_uint32 *out_length, struct dcache *adc,
		struct osi_file *fP, struct fetchOps **ops, void **rock)
{
    struct rxfs_fetchVariables *v;
    int code, code1;
    afs_int32 length_hi, length, bytes;
#ifdef AFS_64BIT_CLIENT
    afs_size_t tsize;
    afs_size_t lengthFound;     /* as returned from server */
#endif /* AFS_64BIT_CLIENT */

    v = (struct rxfs_fetchVariables *) osi_AllocSmallSpace(sizeof(struct rxfs_fetchVariables));
    if (!v)
        osi_Panic("rxfs_fetchInit: osi_AllocSmallSpace returned NULL\n");
    memset(v, 0, sizeof(struct rxfs_fetchVariables));

    RX_AFS_GUNLOCK();
    v->call = rx_NewCall(tc->id);
    RX_AFS_GLOCK();

#ifdef AFS_64BIT_CLIENT
    length_hi = code = 0;
    if (!afs_serverHasNo64Bit(tc)) {
	tsize = size;
	RX_AFS_GUNLOCK();
	code =
	    StartRXAFS_FetchData64(v->call, (struct AFSFid *)&avc->f.fid.Fid,
					Position, tsize);
	if (code != 0) {
	    RX_AFS_GLOCK();
	    afs_Trace2(afs_iclSetp, CM_TRACE_FETCH64CODE,
                           ICL_TYPE_POINTER, avc, ICL_TYPE_INT32, code);
	} else {
	    bytes = rx_Read(v->call, (char *)&length_hi, sizeof(afs_int32));
	    RX_AFS_GLOCK();
	    if (bytes == sizeof(afs_int32)) {
		length_hi = ntohl(length_hi);
	    } else {
		length_hi = 0;
		code = rx_Error(v->call);
		RX_AFS_GUNLOCK();
		code1 = rx_EndCall(v->call, code);
		RX_AFS_GLOCK();
		v->call = NULL;
	    }
	}
    }
    if (code == RXGEN_OPCODE || afs_serverHasNo64Bit(tc)) {
	if (Position > 0x7FFFFFFF) {
	    code = EFBIG;
	} else {
	    afs_int32 pos;
	    pos = Position;
	    RX_AFS_GUNLOCK();
	    if (!v->call)
		v->call = rx_NewCall(tc->id);
	    code =
		StartRXAFS_FetchData(v->call, (struct AFSFid *)
				     &avc->f.fid.Fid, pos,
				     size);
	    RX_AFS_GLOCK();
	}
	afs_serverSetNo64Bit(tc);
    }
    if (code == 0) {
	RX_AFS_GUNLOCK();
	bytes =
	    rx_Read(v->call, (char *)&length,
		    sizeof(afs_int32));
	RX_AFS_GLOCK();
	if (bytes == sizeof(afs_int32)) {
	    length = ntohl(length);
	} else {
	    code = rx_Error(v->call);
	}
    }
    FillInt64(lengthFound, length_hi, length);
    afs_Trace3(afs_iclSetp, CM_TRACE_FETCH64LENG,
	       ICL_TYPE_POINTER, avc, ICL_TYPE_INT32, code,
	       ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(lengthFound));
#else /* AFS_64BIT_CLIENT */
    RX_AFS_GUNLOCK();
    code =
	StartRXAFS_FetchData(v->call,
			     (struct AFSFid *)&avc->f.fid.Fid,
			     Position, size);
    RX_AFS_GLOCK();
    if (code == 0) {
	RX_AFS_GUNLOCK();
	bytes =
	    rx_Read(v->call, (char *)&length,
		    sizeof(afs_int32));
	RX_AFS_GLOCK();
	if (bytes == sizeof(afs_int32)) {
	    length = ntohl(length);
	} else {
	    code = rx_Error(v->call);
	}
    }
#endif /* AFS_64BIT_CLIENT */
    if (code) {
	osi_FreeSmallSpace(v);
        return code;
    }

    if ( cacheDiskType == AFS_FCACHE_TYPE_UFS ) {
	v->tbuffer = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
	if (!v->tbuffer)
	    osi_Panic("rxfs_fetchInit: osi_AllocLargeSpace for iovecs returned NULL\n");
	osi_Assert(WriteLocked(&adc->lock));
	fP->offset = 0;
	*ops = (struct fetchOps *) &rxfs_fetchUfsOps;
    }
    else {
	afs_Trace4(afs_iclSetp, CM_TRACE_MEMFETCH, ICL_TYPE_POINTER, avc,
		   ICL_TYPE_POINTER, fP, ICL_TYPE_OFFSET,
		   ICL_HANDLE_OFFSET(Position), ICL_TYPE_INT32, length);
	/*
	 * We need to alloc the iovecs on the heap so that they are "pinned"
	 * rather than declare them on the stack - defect 11272
	 */
	v->iov =
	    (struct iovec *)osi_AllocSmallSpace(sizeof(struct iovec) *
						RX_MAXIOVECS);
	if (!v->iov)
	    osi_Panic("afs_CacheFetchProc: osi_AllocSmallSpace for iovecs returned NULL\n");
	*ops = (struct fetchOps *) &rxfs_fetchMemOps;
    }
    *rock = (void *)v;
    *out_length = length;
    return 0;
}


/*!
 * Routine called on fetch; also tells people waiting for data
 *	that more has arrived.
 *
 * \param tc Ptr to the Rx connection structure.
 * \param fP File descriptor for the cache file.
 * \param abase Base offset to fetch.
 * \param adc Ptr to the dcache entry for the file, write-locked.
 * \param avc Ptr to the vcache entry for the file.
 * \param abytesToXferP Set to the number of bytes to xfer.
 *	NOTE: This parameter is only used if AFS_NOSTATS is not defined.
 * \param abytesXferredP Set to the number of bytes actually xferred.
 *	NOTE: This parameter is only used if AFS_NOSTATS is not defined.
 * \param size Amount of data that should be fetched.
 * \param tsmall Ptr to the afs_FetchOutput structure.
 *
 * \note Environment: Nothing interesting.
 */
int
afs_CacheFetchProc(register struct afs_conn *tc,
		    register struct osi_file *fP, afs_size_t abase,
		    struct dcache *adc, struct vcache *avc,
		    afs_size_t * abytesToXferP, afs_size_t * abytesXferredP,
		    afs_int32 size,
		    struct afs_FetchOutput *tsmall)
{
    register afs_int32 code;
    afs_uint32 length;
    afs_uint32 bytesread, byteswritten;
    struct fetchOps *ops = NULL;
    void *rock = NULL;
    int moredata = 0;
    register int offset = 0;

    AFS_STATCNT(CacheFetchProc);

#ifndef AFS_NOSTATS
    (*abytesToXferP) = 0;
    (*abytesXferredP) = 0;
#endif /* AFS_NOSTATS */

    adc->validPos = abase;

    code = rxfs_fetchInit(tc, avc, abase, size, &length, adc, fP, &ops, &rock);
    if ( !code ) do {
	if (moredata) {
	    code = (*ops->more)(rock, &length, &moredata);
	    if ( code )
		break;
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
#ifdef RX_KERNEL_TRACE
	    afs_Trace1(afs_iclSetp, CM_TRACE_TIMESTAMP, ICL_TYPE_STRING,
		       "before rx_Read");
#endif
	    code = (*ops->read)(rock, length, &bytesread);
#ifdef RX_KERNEL_TRACE
	    afs_Trace1(afs_iclSetp, CM_TRACE_TIMESTAMP, ICL_TYPE_STRING,
		       "after rx_Read");
#endif
#ifndef AFS_NOSTATS
	    (*abytesXferredP) += bytesread;
#endif /* AFS_NOSTATS */
	    if ( code ) {
		afs_Trace3(afs_iclSetp, CM_TRACE_FETCH64READ,
			   ICL_TYPE_POINTER, avc, ICL_TYPE_INT32, code,
			   ICL_TYPE_INT32, length);
		code = -34;
		break;
	    }
	    code = (*ops->write)(rock, fP, offset, bytesread, &byteswritten);
	    if ( code )
		break;
	    offset += bytesread;
	    abase += bytesread;
	    length -= bytesread;
	    adc->validPos = abase;
	    if (afs_osi_Wakeup(&adc->validPos) == 0)
		afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAKE, ICL_TYPE_STRING,
			   __FILE__, ICL_TYPE_INT32, __LINE__,
			   ICL_TYPE_POINTER, adc, ICL_TYPE_INT32,
			   adc->dflags);
	}
	code = 0;
    } while (moredata);
    if (!code)
	code = (*ops->close)(rock, avc, adc, tsmall);
    (*ops->destroy)(&rock, code);
    return code;
}

