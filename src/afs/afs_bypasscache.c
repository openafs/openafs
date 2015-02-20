/*
 * COPYRIGHT  ©  2000
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY O
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

 /*
 * Portions Copyright (c) 2008
 * The Linux Box Corporation
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the Linux Box
 * Corporation is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * Linux Box Corporation is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the Linux Box Corporation as to its fitness for any
 * purpose, and without warranty by the Linux Box Corporation
 * of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * Linux Box Corporation shall not be liable for any damages,
 * including special, indirect, incidental, or consequential
 * damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been
 * or is hereafter advised of the possibility of such damages.
 */


#include <afsconfig.h>
#include "afs/param.h"
#if defined(AFS_CACHE_BYPASS) || defined(UKERNEL)
#include "afs/afs_bypasscache.h"

/*
 * afs_bypasscache.c
 *
 */
#include "afs/sysincludes.h" /* Standard vendor system headers */
#include "afs/afsincludes.h" /* Afs-based standard headers */
#include "afs/afs_stats.h"   /* statistics */
#include "afs/nfsclient.h"
#include "rx/rx_globals.h"

#ifndef afs_min
#define afs_min(A,B) ((A)<(B)) ? (A) : (B)
#endif

/* conditional GLOCK macros */
#define COND_GLOCK(var)	\
	do { \
		var = ISAFS_GLOCK(); \
		if(!var) \
			RX_AFS_GLOCK(); \
	} while(0)

#define COND_RE_GUNLOCK(var) \
	do { \
		if(var)	\
			RX_AFS_GUNLOCK(); \
	} while(0)


/* conditional GUNLOCK macros */

#define COND_GUNLOCK(var) \
	do {	\
		var = ISAFS_GLOCK(); \
		if(var)	\
			RX_AFS_GUNLOCK(); \
	} while(0)

#define COND_RE_GLOCK(var) \
	do { \
		if(var)	\
			RX_AFS_GLOCK();	\
	} while(0)


int cache_bypass_strategy   = 	NEVER_BYPASS_CACHE;
int cache_bypass_threshold  =  	AFS_CACHE_BYPASS_DISABLED; /* file size > threshold triggers bypass */
int cache_bypass_prefetch = 1;	/* Should we do prefetching ? */

extern afs_rwlock_t afs_xcbhash;

/*
 * This is almost exactly like the PFlush() routine in afs_pioctl.c,
 * but that routine is static.  We are about to change a file from
 * normal caching to bypass it's caching.  Therefore, we want to
 * free up any cache space in use by the file, and throw out any
 * existing VM pages for the file.  We keep track of the number of
 * times we go back and forth from caching to bypass.
 */
void
afs_TransitionToBypass(struct vcache *avc,
		       afs_ucred_t *acred, int aflags)
{

    afs_int32 code;
    int setDesire = 0;
    int setManual = 0;

    if (!avc)
	return;

    if (aflags & TRANSChangeDesiredBit)
	setDesire = 1;
    if (aflags & TRANSSetManualBit)
	setManual = 1;

#ifdef AFS_BOZONLOCK_ENV
    afs_BozonLock(&avc->pvnLock, avc);	/* Since afs_TryToSmush will do a pvn_vptrunc */
#else
    AFS_GLOCK();
#endif

    ObtainWriteLock(&avc->lock, 925);
    /*
     * Someone may have beat us to doing the transition - we had no lock
     * when we checked the flag earlier.  No cause to panic, just return.
     */
    if (avc->cachingStates & FCSBypass)
	goto done;

    /* If we never cached this, just change state */
    if (setDesire && (!(avc->cachingStates & FCSBypass))) {
	avc->cachingStates |= FCSBypass;
	goto done;
    }

    /* cg2v, try to store any chunks not written 20071204 */
    if (avc->execsOrWriters > 0) {
	struct vrequest *treq = NULL;

	code = afs_CreateReq(&treq, acred);
	if (!code) {
	    code = afs_StoreAllSegments(avc, treq, AFS_SYNC | AFS_LASTSTORE);
	    afs_DestroyReq(treq);
	}
    }

#if 0
    /* also cg2v, don't dequeue the callback */
    ObtainWriteLock(&afs_xcbhash, 956);
    afs_DequeueCallback(avc);
    ReleaseWriteLock(&afs_xcbhash);
#endif
    avc->f.states &= ~(CStatd | CDirty);      /* next reference will re-stat */
    /* now find the disk cache entries */
    afs_TryToSmush(avc, acred, 1);
    osi_dnlc_purgedp(avc);
    if (avc->linkData && !(avc->f.states & CCore)) {
	afs_osi_Free(avc->linkData, strlen(avc->linkData) + 1);
	avc->linkData = NULL;
    }

    avc->cachingStates |= FCSBypass;    /* Set the bypass flag */
    if(setDesire)
	avc->cachingStates |= FCSDesireBypass;
    if(setManual)
	avc->cachingStates |= FCSManuallySet;
    avc->cachingTransitions++;

done:
    ReleaseWriteLock(&avc->lock);
#ifdef AFS_BOZONLOCK_ENV
    afs_BozonUnlock(&avc->pvnLock, avc);
#else
    AFS_GUNLOCK();
#endif
}

/*
 * This is almost exactly like the PFlush() routine in afs_pioctl.c,
 * but that routine is static.  We are about to change a file from
 * bypassing caching to normal caching.  Therefore, we want to
 * throw out any existing VM pages for the file.  We keep track of
 * the number of times we go back and forth from caching to bypass.
 */
void
afs_TransitionToCaching(struct vcache *avc,
		        afs_ucred_t *acred,
			int aflags)
{
    int resetDesire = 0;
    int setManual = 0;

    if (!avc)
	return;

    if (aflags & TRANSChangeDesiredBit)
	resetDesire = 1;
    if (aflags & TRANSSetManualBit)
	setManual = 1;

#ifdef AFS_BOZONLOCK_ENV
    afs_BozonLock(&avc->pvnLock, avc);	/* Since afs_TryToSmush will do a pvn_vptrunc */
#else
    AFS_GLOCK();
#endif
    ObtainWriteLock(&avc->lock, 926);
    /*
     * Someone may have beat us to doing the transition - we had no lock
     * when we checked the flag earlier.  No cause to panic, just return.
     */
    if (!(avc->cachingStates & FCSBypass))
	goto done;

    /* Ok, we actually do need to flush */
    ObtainWriteLock(&afs_xcbhash, 957);
    afs_DequeueCallback(avc);
    avc->f.states &= ~(CStatd | CDirty);	/* next reference will re-stat cache entry */
    ReleaseWriteLock(&afs_xcbhash);
    /* now find the disk cache entries */
    afs_TryToSmush(avc, acred, 1);
    osi_dnlc_purgedp(avc);
    if (avc->linkData && !(avc->f.states & CCore)) {
	afs_osi_Free(avc->linkData, strlen(avc->linkData) + 1);
	avc->linkData = NULL;
    }

    avc->cachingStates &= ~(FCSBypass);    /* Reset the bypass flag */
    if (resetDesire)
	avc->cachingStates &= ~(FCSDesireBypass);
    if (setManual)
	avc->cachingStates |= FCSManuallySet;
    avc->cachingTransitions++;

done:
    ReleaseWriteLock(&avc->lock);
#ifdef AFS_BOZONLOCK_ENV
    afs_BozonUnlock(&avc->pvnLock, avc);
#else
    AFS_GUNLOCK();
#endif
}

/* In the case where there's an error in afs_NoCacheFetchProc or
 * afs_PrefetchNoCache, all of the pages they've been passed need
 * to be unlocked.
 */
#ifdef UKERNEL
typedef void * bypass_page_t;

#define unlock_and_release_pages(auio)
#define release_full_page(pp, pageoff)

#else
typedef struct page * bypass_page_t;

#define unlock_and_release_pages(auio) \
    do { \
	struct iovec *ciov;	\
	bypass_page_t pp; \
	afs_int32 iovmax; \
	afs_int32 iovno = 0; \
	ciov = auio->uio_iov; \
	iovmax = auio->uio_iovcnt - 1;	\
	pp = (bypass_page_t) ciov->iov_base;	\
	while(1) { \
	    if (pp) { \
		if (PageLocked(pp)) \
		    unlock_page(pp);	\
		put_page(pp); /* decrement refcount */ \
	    } \
	    iovno++; \
	    if(iovno > iovmax) \
		break; \
	    ciov = (auio->uio_iov + iovno);	\
	    pp = (bypass_page_t) ciov->iov_base;	\
	} \
    } while(0)

#define release_full_page(pp, pageoff)			\
    do { \
	/* this is appropriate when no caller intends to unlock \
	 * and release the page */ \
	SetPageUptodate(pp); \
	if(PageLocked(pp)) \
	    unlock_page(pp); \
	else \
	    afs_warn("afs_NoCacheFetchProc: page not locked!\n"); \
	put_page(pp); /* decrement refcount */ \
    } while(0)
#endif

static void
afs_bypass_copy_page(bypass_page_t pp, int pageoff, struct iovec *rxiov,
	int iovno, int iovoff, struct uio *auio, int curiov, int partial)
{
    char *address;
    int dolen;

    if (partial)
	dolen = auio->uio_iov[curiov].iov_len - pageoff;
    else
	dolen = rxiov[iovno].iov_len - iovoff;

#if !defined(UKERNEL)
# if defined(KMAP_ATOMIC_TAKES_NO_KM_TYPE)
    address = kmap_atomic(pp);
# else
    address = kmap_atomic(pp, KM_USER0);
# endif
#else
    address = pp;
#endif
    memcpy(address + pageoff, (char *)(rxiov[iovno].iov_base) + iovoff, dolen);
#if !defined(UKERNEL)
# if defined(KMAP_ATOMIC_TAKES_NO_KM_TYPE)
    kunmap_atomic(address);
# else
    kunmap_atomic(address, KM_USER0);
# endif
#endif
}

/* no-cache prefetch routine */
static afs_int32
afs_NoCacheFetchProc(struct rx_call *acall,
                     struct vcache *avc,
		     struct uio *auio,
                     afs_int32 release_pages,
		     afs_int32 size)
{
    afs_int32 length;
    afs_int32 code;
    int moredata, iovno, iovoff, iovmax, result, locked;
    struct iovec *ciov;
    struct iovec *rxiov;
    int nio = 0;
    bypass_page_t pp;

    int curpage, bytes;
    int pageoff;

    rxiov = osi_AllocSmallSpace(sizeof(struct iovec) * RX_MAXIOVECS);
    ciov = auio->uio_iov;
    pp = (bypass_page_t) ciov->iov_base;
    iovmax = auio->uio_iovcnt - 1;
    iovno = iovoff = result = 0;

    do {
	COND_GUNLOCK(locked);
	code = rx_Read(acall, (char *)&length, sizeof(afs_int32));
	COND_RE_GLOCK(locked);
	if (code != sizeof(afs_int32)) {
	    result = EIO;
	    afs_warn("Preread error. code: %d instead of %d\n",
		code, (int)sizeof(afs_int32));
	    unlock_and_release_pages(auio);
	    goto done;
	} else
	    length = ntohl(length);

	if (length > size) {
	    result = EIO;
	    afs_warn("Preread error. Got length %d, which is greater than size %d\n",
	             length, size);
	    unlock_and_release_pages(auio);
	    goto done;
	}

	/* If we get a 0 length reply, time to cleanup and return */
	if (length == 0) {
	    unlock_and_release_pages(auio);
	    result = 0;
	    goto done;
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

	for (curpage = 0; curpage <= iovmax; curpage++) {
	    pageoff = 0;
	    /* properly, this should track uio_resid, not a fixed page size! */
	    while (pageoff < auio->uio_iov[curpage].iov_len) {
		/* If no more iovs, issue new read. */
		if (iovno >= nio) {
		    COND_GUNLOCK(locked);
		    bytes = rx_Readv(acall, rxiov, &nio, RX_MAXIOVECS, length);
		    COND_RE_GLOCK(locked);
		    if (bytes < 0) {
		        afs_warn("afs_NoCacheFetchProc: rx_Read error. Return code was %d\n", bytes);
			result = bytes;
			unlock_and_release_pages(auio);
			goto done;
		    } else if (bytes == 0) {
			/* we failed to read the full length */
			result = EIO;
			afs_warn("afs_NoCacheFetchProc: rx_Read returned zero. Aborting.\n");
			unlock_and_release_pages(auio);
			goto done;
		    }
		    size -= bytes;
		    iovno = 0;
		}
		pp = (bypass_page_t)auio->uio_iov[curpage].iov_base;
		if (pageoff + (rxiov[iovno].iov_len - iovoff) <= auio->uio_iov[curpage].iov_len) {
		    /* Copy entire (or rest of) current iovec into current page */
		    if (pp)
			afs_bypass_copy_page(pp, pageoff, rxiov, iovno, iovoff, auio, curpage, 0);
		    length -= (rxiov[iovno].iov_len - iovoff);
		    pageoff += rxiov[iovno].iov_len - iovoff;
		    iovno++;
		    iovoff = 0;
		} else {
		    /* Copy only what's needed to fill current page */
		    if (pp)
			afs_bypass_copy_page(pp, pageoff, rxiov, iovno, iovoff, auio, curpage, 1);
		    length -= (auio->uio_iov[curpage].iov_len - pageoff);
		    iovoff += auio->uio_iov[curpage].iov_len - pageoff;
		    pageoff = auio->uio_iov[curpage].iov_len;
		}

		/* we filled a page, or this is the last page.  conditionally release it */
		if (pp && ((pageoff == auio->uio_iov[curpage].iov_len &&
			    release_pages) || (length == 0 && iovno >= nio)))
		    release_full_page(pp, pageoff);

		if (length == 0 && iovno >= nio)
		    goto done;
	    }
	}
    } while (moredata);

done:
    osi_FreeSmallSpace(rxiov);
    return result;
}


/* dispatch a no-cache read request */
afs_int32
afs_ReadNoCache(struct vcache *avc,
		struct nocache_read_request *bparms,
		afs_ucred_t *acred)
{
    afs_int32 code;
    afs_int32 bcnt;
    struct brequest *breq;
    struct vrequest *areq = NULL;

    if (avc && avc->vc_error) {
	code = EIO;
	afs_warn("afs_ReadNoCache VCache Error!\n");
	goto cleanup;
    }

    AFS_GLOCK();
    /* the receiver will free areq */
    code = afs_CreateReq(&areq, acred);
    if (code) {
	afs_warn("afs_ReadNoCache afs_CreateReq error!\n");
    } else {
	code = afs_VerifyVCache(avc, areq);
	if (code) {
	    afs_warn("afs_ReadNoCache Failed to verify VCache!\n");
	}
    }
    AFS_GUNLOCK();

    if (code) {
	code = afs_CheckCode(code, areq, 11);	/* failed to get it */
	goto cleanup;
    }

    bparms->areq = areq;

    /* and queue this one */
    bcnt = 1;
    AFS_GLOCK();
    while(bcnt < 20) {
	breq = afs_BQueue(BOP_FETCH_NOCACHE, avc, B_DONTWAIT, 0, acred, 1, 1,
			  bparms, (void *)0, (void *)0);
	if(breq != 0) {
	    code = 0;
	    break;
	}
	afs_osi_Wait(10 * bcnt, 0, 0);
    }
    AFS_GUNLOCK();

    if(!breq) {
    	code = EBUSY;
	goto cleanup;
    }

    return code;

cleanup:
    /* If there's a problem before we queue the request, we need to
     * do everything that would normally happen when the request was
     * processed, like unlocking the pages and freeing memory.
     */
    unlock_and_release_pages(bparms->auio);
    AFS_GLOCK();
    afs_DestroyReq(areq);
    AFS_GUNLOCK();
    osi_Free(bparms->auio->uio_iov,
	     bparms->auio->uio_iovcnt * sizeof(struct iovec));
    osi_Free(bparms->auio, sizeof(struct uio));
    osi_Free(bparms, sizeof(struct nocache_read_request));
    return code;
}


/* Cannot have static linkage--called from BPrefetch (afs_daemons) */
afs_int32
afs_PrefetchNoCache(struct vcache *avc,
		    afs_ucred_t *acred,
		    struct nocache_read_request *bparms)
{
    struct uio *auio;
#ifndef UKERNEL
    struct iovec *iovecp;
#endif
    struct vrequest *areq;
    afs_int32 code = 0;
    struct rx_connection *rxconn;
#ifdef AFS_64BIT_CLIENT
    afs_int32 length_hi, bytes, locked;
#endif

    struct afs_conn *tc;
    struct rx_call *tcall;
    struct tlocal1 {
	struct AFSVolSync tsync;
	struct AFSFetchStatus OutStatus;
	struct AFSCallBack CallBack;
    };
    struct tlocal1 *tcallspec;

    auio = bparms->auio;
    areq = bparms->areq;
#ifndef UKERNEL
    iovecp = auio->uio_iov;
#endif

    tcallspec = (struct tlocal1 *) osi_Alloc(sizeof(struct tlocal1));
    do {
      tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK /* ignored */, &rxconn);
	if (tc) {
	    avc->callback = tc->srvr->server;
	    tcall = rx_NewCall(rxconn);
#ifdef AFS_64BIT_CLIENT
	    if (!afs_serverHasNo64Bit(tc)) {
		code = StartRXAFS_FetchData64(tcall,
					      (struct AFSFid *) &avc->f.fid.Fid,
					      auio->uio_offset,
					      bparms->length);
		if (code == 0) {
		    COND_GUNLOCK(locked);
		    bytes = rx_Read(tcall, (char *)&length_hi,
				    sizeof(afs_int32));
		    COND_RE_GLOCK(locked);

		    if (bytes != sizeof(afs_int32)) {
			length_hi = 0;
			code = rx_Error(tcall);
			COND_GUNLOCK(locked);
			code = rx_EndCall(tcall, code);
			COND_RE_GLOCK(locked);
			tcall = NULL;
		    }
		}
	    } /* afs_serverHasNo64Bit */
	    if (code == RXGEN_OPCODE || afs_serverHasNo64Bit(tc)) {
		if (auio->uio_offset > 0x7FFFFFFF) {
		    code = EFBIG;
		} else {
		    afs_int32 pos;
		    pos = auio->uio_offset;
		    COND_GUNLOCK(locked);
		    if (!tcall)
			tcall = rx_NewCall(rxconn);
		    code = StartRXAFS_FetchData(tcall,
					(struct AFSFid *) &avc->f.fid.Fid,
					pos, bparms->length);
		    COND_RE_GLOCK(locked);
		}
		afs_serverSetNo64Bit(tc);
	    }
#else
	    code = StartRXAFS_FetchData(tcall,
			                (struct AFSFid *) &avc->f.fid.Fid,
					auio->uio_offset, bparms->length);
#endif
	    if (code == 0) {
		code = afs_NoCacheFetchProc(tcall, avc, auio,
				            1 /* release_pages */,
					    bparms->length);
	    } else {
		afs_warn("BYPASS: StartRXAFS_FetchData failed: %d\n", code);
		unlock_and_release_pages(auio);
		afs_PutConn(tc, rxconn, SHARED_LOCK);
		goto done;
	    }
	    if (code == 0) {
		code = EndRXAFS_FetchData(tcall, &tcallspec->OutStatus,
					  &tcallspec->CallBack,
					  &tcallspec->tsync);
	    } else {
		afs_warn("BYPASS: NoCacheFetchProc failed: %d\n", code);
	    }
	    code = rx_EndCall(tcall, code);
	} else {
	    afs_warn("BYPASS: No connection.\n");
	    code = -1;
	    unlock_and_release_pages(auio);
	    goto done;
	}
    } while (afs_Analyze(tc, rxconn, code, &avc->f.fid, areq,
						 AFS_STATS_FS_RPCIDX_FETCHDATA,
						 SHARED_LOCK,0));
done:
    /*
     * Copy appropriate fields into vcache
     */

    if (!code)
	afs_ProcessFS(avc, &tcallspec->OutStatus, areq);

    osi_Free(areq, sizeof(struct vrequest));
    osi_Free(tcallspec, sizeof(struct tlocal1));
    osi_Free(bparms, sizeof(struct nocache_read_request));
#ifndef UKERNEL
    /* in UKERNEL, the "pages" are passed in */
    osi_Free(iovecp, auio->uio_iovcnt * sizeof(struct iovec));
    osi_Free(auio, sizeof(struct uio));
#endif
    return code;
}
#endif
