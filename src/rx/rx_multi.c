
/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

#include <afs/param.h>

#ifdef	KERNEL
#include "../rx/rx_kernel.h"
#include "../rx/rx_multi.h"
#else /* KERNEL */
# include "rx_user.h"
# include "rx_multi.h"
# include "rx_internal.h"
#endif /* KERNEL */

/* multi.c and multi.h, together with some rxgen hooks, provide a way of making multiple, but similar, rx calls to multiple hosts simultaneously */

struct multi_handle *multi_Init(conns, nConns)
    struct rx_connection **conns;
    register int nConns;
{
    void multi_Ready();
    register struct rx_call **calls;
    register short *ready;
    register struct multi_handle *mh;
    register int i;
    /* Note:  all structures that are possibly referenced by other processes must be allocated.  In some kernels variables allocated on a process stack will not be accessible to other processes */
    calls = (struct rx_call **)osi_Alloc(sizeof (struct rx_call *) * nConns);
    ready = (short *) osi_Alloc(sizeof(short *) * nConns);
    mh = (struct multi_handle *) osi_Alloc(sizeof (struct multi_handle));
    if (!calls || !ready || !mh) osi_Panic("multi_Rx: no mem\n");
    mh->calls = calls;
    mh->nextReady = mh->firstNotReady = mh->ready = ready;
    mh->nReady = 0;
    mh->nConns = nConns;
#ifdef RX_ENABLE_LOCKS
    MUTEX_INIT(&mh->lock, "rx_multi_lock", MUTEX_DEFAULT, 0);
    CV_INIT(&mh->cv, "rx_multi_cv", CV_DEFAULT, 0);
#endif /* RX_ENABLE_LOCKS */
    for (i=0; i<nConns; i++) {
	register struct rx_call *call;
	call = mh->calls[i] = rx_NewCall(conns[i]);
	rx_SetArrivalProc(call, multi_Ready, (VOID *)mh, (VOID *)i);
    }
    return mh;
}

/* Return the user's connection index of the most recently ready call; that is, a call that has received at least one reply packet */
int multi_Select(mh)
    register struct multi_handle *mh;
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
void multi_Ready(call, mh, index)
    register struct rx_call *call;
    register struct multi_handle *mh;
    register int index;
{
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
void multi_Finalize(mh)
register struct multi_handle *mh;
{
    register int i;
    register int nCalls = mh->nConns;
    for (i=0; i<nCalls; i++) {
	register struct rx_call *call = mh->calls[i];
	if (call) rx_EndCall(call, RX_USER_ABORT);
    }
#ifdef RX_ENABLE_LOCKS
    MUTEX_DESTROY(&mh->lock);
    CV_DESTROY(&mh->cv);
#endif /* RX_ENABLE_LOCKS */
    osi_Free(mh->calls, sizeof (struct rx_call *) * nCalls);
    osi_Free(mh->ready, sizeof(short *) * nCalls);
    osi_Free(mh, sizeof(struct multi_handle));
}

/* ignores all remaining multiRx calls */
void multi_Finalize_Ignore(mh)
register struct multi_handle *mh;
{
    register int i;
    register int nCalls = mh->nConns;
    for (i=0; i<nCalls; i++) {
	register struct rx_call *call = mh->calls[i];
	if (call) rx_EndCall(call, 0);
    }
#ifdef RX_ENABLE_LOCKS
    MUTEX_DESTROY(&mh->lock);
    CV_DESTROY(&mh->cv);
#endif /* RX_ENABLE_LOCKS */
    osi_Free(mh->calls, sizeof (struct rx_call *) * nCalls);
    osi_Free(mh->ready, sizeof(short *) * nCalls);
    osi_Free(mh, sizeof(struct multi_handle));
}
