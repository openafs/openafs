/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef	KERNEL
#include "afs/sysincludes.h"
#include "rx/rx.h"
#else /* KERNEL */
# include "rx.h"
#endif /* KERNEL */

/*
 * multi.c and multi.h, together with some rxgen hooks, provide a way of
 * making multiple, but similar, rx calls to multiple hosts simultaneously
 */

struct multi_handle *
multi_Init(struct rx_connection **conns, register int nConns)
{
    register struct rx_call **calls;
    register short *ready;
    register struct multi_handle *mh;
    register int i;

    /*
     * Note: all structures that are possibly referenced by other
     * processes must be allocated.  In some kernels variables allocated on
     * a process stack will not be accessible to other processes
     */

    calls = (struct rx_call **)osi_Alloc(sizeof(struct rx_call *) * nConns);
    ready = (short *)osi_Alloc(sizeof(short *) * nConns);
    mh = (struct multi_handle *)osi_Alloc(sizeof(struct multi_handle));
    if (!calls || !ready || !mh)
	osi_Panic("multi_Rx: no mem\n");
    mh->calls = calls;
    mh->nextReady = mh->firstNotReady = mh->ready = ready;
    mh->nReady = 0;
    mh->nConns = nConns;
#ifdef RX_ENABLE_LOCKS
    MUTEX_INIT(&mh->lock, "rx_multi_lock", MUTEX_DEFAULT, 0);
    CV_INIT(&mh->cv, "rx_multi_cv", CV_DEFAULT, 0);
#endif /* RX_ENABLE_LOCKS */
    for (i = 0; i < nConns; i++) {
	register struct rx_call *call;
	call = mh->calls[i] = rx_NewCall(conns[i]);
	rx_SetArrivalProc(call, multi_Ready, (void *) mh, i);
    }
    return mh;
}

/* Return the user's connection index of the most recently ready call; that is, a call that has received at least one reply packet */
int
multi_Select(register struct multi_handle *mh)
{
    int index;
    SPLVAR;
    NETPRI;
#ifdef RX_ENABLE_LOCKS
    MUTEX_ENTER(&mh->lock);
#endif /* RX_ENABLE_LOCKS */
    while (mh->nextReady == mh->firstNotReady) {
	if (mh->nReady == mh->nConns) {
#ifdef RX_ENABLE_LOCKS
	    MUTEX_EXIT(&mh->lock);
#endif /* RX_ENABLE_LOCKS */
	    USERPRI;
	    return -1;
	}
#ifdef RX_ENABLE_LOCKS
	CV_WAIT(&mh->cv, &mh->lock);
#else /* RX_ENABLE_LOCKS */
	osi_rxSleep(mh);
#endif /* RX_ENABLE_LOCKS */
    }
    index = *(mh->nextReady);
    (mh->nextReady) += 1;
#ifdef RX_ENABLE_LOCKS
    MUTEX_EXIT(&mh->lock);
#endif /* RX_ENABLE_LOCKS */
    USERPRI;
    return index;
}

/* Called by Rx when the first reply packet of a call is received, or the call is aborted. */
void
multi_Ready(register struct rx_call *call, register void *amh,
	    register int index)
{
    register struct multi_handle *mh = (struct multi_handle *)amh;
#ifdef RX_ENABLE_LOCKS
    MUTEX_ENTER(&mh->lock);
#endif /* RX_ENABLE_LOCKS */
    *mh->firstNotReady++ = index;
    mh->nReady++;
#ifdef RX_ENABLE_LOCKS
    CV_SIGNAL(&mh->cv);
    MUTEX_EXIT(&mh->lock);
#else /* RX_ENABLE_LOCKS */
    osi_rxWakeup(mh);
#endif /* RX_ENABLE_LOCKS */
}

/* Called when the multi rx call is over, or when the user aborts it (by using the macro multi_Abort) */
void
multi_Finalize(register struct multi_handle *mh)
{
    register int i;
    register int nCalls = mh->nConns;
    for (i = 0; i < nCalls; i++) {
	register struct rx_call *call = mh->calls[i];
	if (call)
	    rx_EndCall(call, RX_USER_ABORT);
    }
#ifdef RX_ENABLE_LOCKS
    MUTEX_DESTROY(&mh->lock);
    CV_DESTROY(&mh->cv);
#endif /* RX_ENABLE_LOCKS */
    osi_Free(mh->calls, sizeof(struct rx_call *) * nCalls);
    osi_Free(mh->ready, sizeof(short *) * nCalls);
    osi_Free(mh, sizeof(struct multi_handle));
}

/* ignores all remaining multiRx calls */
void
multi_Finalize_Ignore(register struct multi_handle *mh)
{
    register int i;
    register int nCalls = mh->nConns;
    for (i = 0; i < nCalls; i++) {
	register struct rx_call *call = mh->calls[i];
	if (call)
	    rx_EndCall(call, 0);
    }
#ifdef RX_ENABLE_LOCKS
    MUTEX_DESTROY(&mh->lock);
    CV_DESTROY(&mh->cv);
#endif /* RX_ENABLE_LOCKS */
    osi_Free(mh->calls, sizeof(struct rx_call *) * nCalls);
    osi_Free(mh->ready, sizeof(short *) * nCalls);
    osi_Free(mh, sizeof(struct multi_handle));
}
