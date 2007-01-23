 /*
  * Copyright 2000, International Business Machines Corporation and others.
  * All Rights Reserved.
  * 
  * This software has been released under the terms of the IBM Public
  * License.  For details, see the LICENSE file in the top-level source
  * directory or online at http://www.openafs.org/dl/license10.html
  */

#include <afsconfig.h>
#ifdef KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header$");

#ifdef KERNEL
#ifndef UKERNEL
#ifdef RX_KERNEL_TRACE
#include "rx_kcommon.h"
#endif
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#include "afs/sysincludes.h"
#else
#include "h/types.h"
#include "h/time.h"
#include "h/stat.h"
#if defined(AFS_AIX_ENV) || defined(AFS_AUX_ENV) || defined(AFS_SUN5_ENV) 
#include "h/systm.h"
#endif
#ifdef	AFS_OSF_ENV
#include <net/net_globals.h>
#endif /* AFS_OSF_ENV */
#ifdef AFS_LINUX20_ENV
#include "h/socket.h"
#endif
#include "netinet/in.h"
#if defined(AFS_SGI_ENV)
#include "afs/sysincludes.h"
#endif
#endif
#include "afs/afs_args.h"
#include "afs/afs_osi.h"
#if	(defined(AFS_AUX_ENV) || defined(AFS_AIX_ENV))
#include "h/systm.h"
#endif
#else /* !UKERNEL */
#include "afs/sysincludes.h"
#endif /* !UKERNEL */
#ifdef RXDEBUG
#undef RXDEBUG			/* turn off debugging */
#endif /* RXDEBUG */

#include "rx_kmutex.h"
#include "rx/rx_kernel.h"
#include "rx/rx_clock.h"
#include "rx/rx_queue.h"
#include "rx/rx.h"
#include "rx/rx_globals.h"
#include "afs/lock.h"
#include "afsint.h"
#ifdef  AFS_OSF_ENV
#undef kmem_alloc
#undef kmem_free
#undef mem_alloc
#undef mem_free
#undef register
#endif /* AFS_OSF_ENV */
#else /* KERNEL */
# include <sys/types.h>
#ifdef AFS_NT40_ENV
# include <winsock2.h>
#else /* !AFS_NT40_ENV */
# include <sys/socket.h>
# include <sys/file.h>
# include <netdb.h>
# include <netinet/in.h>
# include <sys/stat.h>
# include <sys/time.h>
#endif /* !AFS_NT40_ENV */
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
# include "rx_user.h"
# include "rx_clock.h"
# include "rx_queue.h"
# include "rx.h"
# include "rx_globals.h"
#endif /* KERNEL */

#ifdef RX_LOCKS_DB
/* rxdb_fileID is used to identify the lock location, along with line#. */
static int rxdb_fileID = RXDB_FILE_RX_RDWR;
#endif /* RX_LOCKS_DB */
/* rxi_ReadProc -- internal version.
 *
 * LOCKS USED -- called at netpri with rx global lock and call->lock held.
 */
int
rxi_ReadProc(register struct rx_call *call, register char *buf,
	     register int nbytes)
{
    register struct rx_packet *cp = call->currentPacket;
    register struct rx_packet *rp;
    register int requestCount;
    register unsigned int t;

/* XXXX took out clock_NewTime from here.  Was it needed? */
    requestCount = nbytes;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (queue_IsNotEmpty(&call->iovq)) {
        rxi_FreePackets(0, &call->iovq);
    }

    do {
	if (call->nLeft == 0) {
	    /* Get next packet */
	    for (;;) {
		if (call->error || (call->mode != RX_MODE_RECEIVING)) {
		    if (call->error) {
			return 0;
		    }
		    if (call->mode == RX_MODE_SENDING) {
			rxi_FlushWrite(call);
			continue;
		    }
		}
		if (queue_IsNotEmpty(&call->rq)) {
		    /* Check that next packet available is next in sequence */
		    rp = queue_First(&call->rq, rx_packet);
		    if (rp->header.seq == call->rnext) {
			afs_int32 error;
			register struct rx_connection *conn = call->conn;
			queue_Remove(rp);

			/* RXS_CheckPacket called to undo RXS_PreparePacket's
			 * work.  It may reduce the length of the packet by up
			 * to conn->maxTrailerSize, to reflect the length of the
			 * data + the header. */
			if ((error =
			     RXS_CheckPacket(conn->securityObject, call,
					     rp))) {
			    /* Used to merely shut down the call, but now we 
			     * shut down the whole connection since this may 
			     * indicate an attempt to hijack it */

			    MUTEX_EXIT(&call->lock);
			    rxi_ConnectionError(conn, error);
			    MUTEX_ENTER(&conn->conn_data_lock);
			    rp = rxi_SendConnectionAbort(conn, rp, 0, 0);
			    MUTEX_EXIT(&conn->conn_data_lock);
			    rxi_FreePacket(rp);
			    MUTEX_ENTER(&call->lock);

			    return 0;
			}
			call->rnext++;
			cp = call->currentPacket = rp;
			call->curvec = 1;	/* 0th vec is always header */
			/* begin at the beginning [ more or less ], continue 
			 * on until the end, then stop. */
			call->curpos =
			    (char *)cp->wirevec[1].iov_base +
			    call->conn->securityHeaderSize;
			call->curlen =
			    cp->wirevec[1].iov_len -
			    call->conn->securityHeaderSize;

			/* Notice that this code works correctly if the data
			 * size is 0 (which it may be--no reply arguments from
			 * server, for example).  This relies heavily on the
			 * fact that the code below immediately frees the packet
			 * (no yields, etc.).  If it didn't, this would be a
			 * problem because a value of zero for call->nLeft
			 * normally means that there is no read packet */
			call->nLeft = cp->length;
			hadd32(call->bytesRcvd, cp->length);

			/* Send a hard ack for every rxi_HardAckRate+1 packets
			 * consumed. Otherwise schedule an event to send
			 * the hard ack later on.
			 */
			call->nHardAcks++;
			if (!(call->flags & RX_CALL_RECEIVE_DONE)) {
			    if (call->nHardAcks > (u_short) rxi_HardAckRate) {
				rxevent_Cancel(call->delayedAckEvent, call,
					       RX_CALL_REFCOUNT_DELAY);
				rxi_SendAck(call, 0, 0, RX_ACK_DELAY, 0);
			    } else {
				struct clock when;
				clock_GetTime(&when);
				/* Delay to consolidate ack packets */
				clock_Add(&when, &rx_hardAckDelay);
				if (!call->delayedAckEvent
				    || clock_Gt(&call->delayedAckEvent->
						eventTime, &when)) {
				    rxevent_Cancel(call->delayedAckEvent,
						   call,
						   RX_CALL_REFCOUNT_DELAY);
				    CALL_HOLD(call, RX_CALL_REFCOUNT_DELAY);
				    call->delayedAckEvent =
					rxevent_Post(&when,
						     rxi_SendDelayedAck, call,
						     0);
				}
			    }
			}
			break;
		    }
		}

/*
MTUXXX  doesn't there need to be an "else" here ??? 
*/
		/* Are there ever going to be any more packets? */
		if (call->flags & RX_CALL_RECEIVE_DONE) {
		    return requestCount - nbytes;
		}
		/* Wait for in-sequence packet */
		call->flags |= RX_CALL_READER_WAIT;
		clock_NewTime();
		call->startWait = clock_Sec();
		while (call->flags & RX_CALL_READER_WAIT) {
#ifdef	RX_ENABLE_LOCKS
		    CV_WAIT(&call->cv_rq, &call->lock);
#else
		    osi_rxSleep(&call->rq);
#endif
		}

		call->startWait = 0;
#ifdef RX_ENABLE_LOCKS
		if (call->error) {
		    return 0;
		}
#endif /* RX_ENABLE_LOCKS */
	    }
	} else
	    /* assert(cp); */
	    /* MTUXXX  this should be replaced by some error-recovery code before shipping */
	    /* yes, the following block is allowed to be the ELSE clause (or not) */
	    /* It's possible for call->nLeft to be smaller than any particular
	     * iov_len.  Usually, recvmsg doesn't change the iov_len, since it
	     * reflects the size of the buffer.  We have to keep track of the
	     * number of bytes read in the length field of the packet struct.  On
	     * the final portion of a received packet, it's almost certain that
	     * call->nLeft will be smaller than the final buffer. */
	    while (nbytes && cp) {
		t = MIN((int)call->curlen, nbytes);
		t = MIN(t, (int)call->nLeft);
		memcpy(buf, call->curpos, t);
		buf += t;
		nbytes -= t;
		call->curpos += t;
		call->curlen -= t;
		call->nLeft -= t;

		if (!call->nLeft) {
		    /* out of packet.  Get another one. */
		    rxi_FreePacket(cp);
		    cp = call->currentPacket = (struct rx_packet *)0;
		} else if (!call->curlen) {
		    /* need to get another struct iov */
		    if (++call->curvec >= cp->niovecs) {
			/* current packet is exhausted, get ready for another */
			/* don't worry about curvec and stuff, they get set somewhere else */
			rxi_FreePacket(cp);
			cp = call->currentPacket = (struct rx_packet *)0;
			call->nLeft = 0;
		    } else {
			call->curpos =
			    (char *)cp->wirevec[call->curvec].iov_base;
			call->curlen = cp->wirevec[call->curvec].iov_len;
		    }
		}
	    }
	if (!nbytes) {
	    /* user buffer is full, return */
	    return requestCount;
	}

    } while (nbytes);

    return requestCount;
}

int
rx_ReadProc(struct rx_call *call, char *buf, int nbytes)
{
    int bytes;
    int tcurlen;
    int tnLeft;
    char *tcurpos;
    SPLVAR;

    /*
     * Free any packets from the last call to ReadvProc/WritevProc.
     * We do not need the lock because the receiver threads only
     * touch the iovq when the RX_CALL_IOVEC_WAIT flag is set, and the
     * RX_CALL_IOVEC_WAIT is always cleared before returning from
     * ReadvProc/WritevProc.
     */
    if (!queue_IsEmpty(&call->iovq)) {
        rxi_FreePackets(0, &call->iovq);
    }

    /*
     * Most common case, all of the data is in the current iovec.
     * We do not need the lock because this is the only thread that
     * updates the curlen, curpos, nLeft fields.
     *
     * We are relying on nLeft being zero unless the call is in receive mode.
     */
    tcurlen = call->curlen;
    tnLeft = call->nLeft;
    if (!call->error && tcurlen > nbytes && tnLeft > nbytes) {
	tcurpos = call->curpos;
	memcpy(buf, tcurpos, nbytes);
	call->curpos = tcurpos + nbytes;
	call->curlen = tcurlen - nbytes;
	call->nLeft = tnLeft - nbytes;
	return nbytes;
    }

    NETPRI;
    MUTEX_ENTER(&call->lock);
    bytes = rxi_ReadProc(call, buf, nbytes);
    MUTEX_EXIT(&call->lock);
    USERPRI;
    return bytes;
}

/* Optimization for unmarshalling 32 bit integers */
int
rx_ReadProc32(struct rx_call *call, afs_int32 * value)
{
    int bytes;
    int tcurlen;
    int tnLeft;
    char *tcurpos;
    SPLVAR;

    /*
     * Free any packets from the last call to ReadvProc/WritevProc.
     * We do not need the lock because the receiver threads only
     * touch the iovq when the RX_CALL_IOVEC_WAIT flag is set, and the
     * RX_CALL_IOVEC_WAIT is always cleared before returning from
     * ReadvProc/WritevProc.
     */
    if (!queue_IsEmpty(&call->iovq)) {
	rxi_FreePackets(0, &call->iovq);
    }

    /*
     * Most common case, all of the data is in the current iovec.
     * We do not need the lock because this is the only thread that
     * updates the curlen, curpos, nLeft fields.
     *
     * We are relying on nLeft being zero unless the call is in receive mode.
     */
    tcurlen = call->curlen;
    tnLeft = call->nLeft;
    if (!call->error && tcurlen >= sizeof(afs_int32)
	&& tnLeft >= sizeof(afs_int32)) {
	tcurpos = call->curpos;
	memcpy((char *)value, tcurpos, sizeof(afs_int32));
	call->curpos = tcurpos + sizeof(afs_int32);
	call->curlen = (u_short)(tcurlen - sizeof(afs_int32));
	call->nLeft = (u_short)(tnLeft - sizeof(afs_int32));
	return sizeof(afs_int32);
    }

    NETPRI;
    MUTEX_ENTER(&call->lock);
    bytes = rxi_ReadProc(call, (char *)value, sizeof(afs_int32));
    MUTEX_EXIT(&call->lock);
    USERPRI;
    return bytes;
}

/* rxi_FillReadVec
 *
 * Uses packets in the receive queue to fill in as much of the
 * current iovec as possible. Does not block if it runs out
 * of packets to complete the iovec. Return true if an ack packet
 * was sent, otherwise return false */
int
rxi_FillReadVec(struct rx_call *call, afs_uint32 serial)
{
    int didConsume = 0;
    int didHardAck = 0;
    register unsigned int t;
    struct rx_packet *rp;
    struct rx_packet *curp;
    struct iovec *call_iov;
    struct iovec *cur_iov = NULL;

    curp = call->currentPacket;
    if (curp) {
	cur_iov = &curp->wirevec[call->curvec];
    }
    call_iov = &call->iov[call->iovNext];

    while (!call->error && call->iovNBytes && call->iovNext < call->iovMax) {
	if (call->nLeft == 0) {
	    /* Get next packet */
	    if (queue_IsNotEmpty(&call->rq)) {
		/* Check that next packet available is next in sequence */
		rp = queue_First(&call->rq, rx_packet);
		if (rp->header.seq == call->rnext) {
		    afs_int32 error;
		    register struct rx_connection *conn = call->conn;
		    queue_Remove(rp);

		    /* RXS_CheckPacket called to undo RXS_PreparePacket's
		     * work.  It may reduce the length of the packet by up
		     * to conn->maxTrailerSize, to reflect the length of the
		     * data + the header. */
		    if ((error =
			 RXS_CheckPacket(conn->securityObject, call, rp))) {
			/* Used to merely shut down the call, but now we 
			 * shut down the whole connection since this may 
			 * indicate an attempt to hijack it */

			MUTEX_EXIT(&call->lock);
			rxi_ConnectionError(conn, error);
			MUTEX_ENTER(&conn->conn_data_lock);
			rp = rxi_SendConnectionAbort(conn, rp, 0, 0);
			MUTEX_EXIT(&conn->conn_data_lock);
			rxi_FreePacket(rp);
			MUTEX_ENTER(&call->lock);

			return 1;
		    }
		    call->rnext++;
		    curp = call->currentPacket = rp;
		    call->curvec = 1;	/* 0th vec is always header */
		    cur_iov = &curp->wirevec[1];
		    /* begin at the beginning [ more or less ], continue 
		     * on until the end, then stop. */
		    call->curpos =
			(char *)curp->wirevec[1].iov_base +
			call->conn->securityHeaderSize;
		    call->curlen =
			curp->wirevec[1].iov_len -
			call->conn->securityHeaderSize;

		    /* Notice that this code works correctly if the data
		     * size is 0 (which it may be--no reply arguments from
		     * server, for example).  This relies heavily on the
		     * fact that the code below immediately frees the packet
		     * (no yields, etc.).  If it didn't, this would be a
		     * problem because a value of zero for call->nLeft
		     * normally means that there is no read packet */
		    call->nLeft = curp->length;
		    hadd32(call->bytesRcvd, curp->length);

		    /* Send a hard ack for every rxi_HardAckRate+1 packets
		     * consumed. Otherwise schedule an event to send
		     * the hard ack later on.
		     */
		    call->nHardAcks++;
		    didConsume = 1;
		    continue;
		}
	    }
	    break;
	}

	/* It's possible for call->nLeft to be smaller than any particular
	 * iov_len.  Usually, recvmsg doesn't change the iov_len, since it
	 * reflects the size of the buffer.  We have to keep track of the
	 * number of bytes read in the length field of the packet struct.  On
	 * the final portion of a received packet, it's almost certain that
	 * call->nLeft will be smaller than the final buffer. */
	while (call->iovNBytes && call->iovNext < call->iovMax && curp) {

	    t = MIN((int)call->curlen, call->iovNBytes);
	    t = MIN(t, (int)call->nLeft);
	    call_iov->iov_base = call->curpos;
	    call_iov->iov_len = t;
	    call_iov++;
	    call->iovNext++;
	    call->iovNBytes -= t;
	    call->curpos += t;
	    call->curlen -= t;
	    call->nLeft -= t;

	    if (!call->nLeft) {
		/* out of packet.  Get another one. */
		queue_Append(&call->iovq, curp);
		curp = call->currentPacket = (struct rx_packet *)0;
	    } else if (!call->curlen) {
		/* need to get another struct iov */
		if (++call->curvec >= curp->niovecs) {
		    /* current packet is exhausted, get ready for another */
		    /* don't worry about curvec and stuff, they get set somewhere else */
		    queue_Append(&call->iovq, curp);
		    curp = call->currentPacket = (struct rx_packet *)0;
		    call->nLeft = 0;
		} else {
		    cur_iov++;
		    call->curpos = (char *)cur_iov->iov_base;
		    call->curlen = cur_iov->iov_len;
		}
	    }
	}
    }

    /* If we consumed any packets then check whether we need to
     * send a hard ack. */
    if (didConsume && (!(call->flags & RX_CALL_RECEIVE_DONE))) {
	if (call->nHardAcks > (u_short) rxi_HardAckRate) {
	    rxevent_Cancel(call->delayedAckEvent, call,
			   RX_CALL_REFCOUNT_DELAY);
	    rxi_SendAck(call, 0, serial, RX_ACK_DELAY, 0);
	    didHardAck = 1;
	} else {
	    struct clock when;
	    clock_GetTime(&when);
	    /* Delay to consolidate ack packets */
	    clock_Add(&when, &rx_hardAckDelay);
	    if (!call->delayedAckEvent
		|| clock_Gt(&call->delayedAckEvent->eventTime, &when)) {
		rxevent_Cancel(call->delayedAckEvent, call,
			       RX_CALL_REFCOUNT_DELAY);
		CALL_HOLD(call, RX_CALL_REFCOUNT_DELAY);
		call->delayedAckEvent =
		    rxevent_Post(&when, rxi_SendDelayedAck, call, 0);
	    }
	}
    }
    return didHardAck;
}


/* rxi_ReadvProc -- internal version.
 *
 * Fills in an iovec with pointers to the packet buffers. All packets
 * except the last packet (new current packet) are moved to the iovq
 * while the application is processing the data.
 *
 * LOCKS USED -- called at netpri with rx global lock and call->lock held.
 */
int
rxi_ReadvProc(struct rx_call *call, struct iovec *iov, int *nio, int maxio,
	      int nbytes)
{
    int requestCount;
    int nextio;

    requestCount = nbytes;
    nextio = 0;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (queue_IsNotEmpty(&call->iovq)) {
        rxi_FreePackets(0, &call->iovq);
    }

    if (call->mode == RX_MODE_SENDING) {
	rxi_FlushWrite(call);
    }

    if (call->error) {
	return 0;
    }

    /* Get whatever data is currently available in the receive queue.
     * If rxi_FillReadVec sends an ack packet then it is possible
     * that we will receive more data while we drop the call lock
     * to send the packet. Set the RX_CALL_IOVEC_WAIT flag
     * here to avoid a race with the receive thread if we send
     * hard acks in rxi_FillReadVec. */
    call->flags |= RX_CALL_IOVEC_WAIT;
    call->iovNBytes = nbytes;
    call->iovMax = maxio;
    call->iovNext = 0;
    call->iov = iov;
    rxi_FillReadVec(call, 0);

    /* if we need more data then sleep until the receive thread has
     * filled in the rest. */
    if (!call->error && call->iovNBytes && call->iovNext < call->iovMax
	&& !(call->flags & RX_CALL_RECEIVE_DONE)) {
	call->flags |= RX_CALL_READER_WAIT;
	clock_NewTime();
	call->startWait = clock_Sec();
	while (call->flags & RX_CALL_READER_WAIT) {
#ifdef	RX_ENABLE_LOCKS
	    CV_WAIT(&call->cv_rq, &call->lock);
#else
	    osi_rxSleep(&call->rq);
#endif
	}
	call->startWait = 0;
    }
    call->flags &= ~RX_CALL_IOVEC_WAIT;
#ifdef RX_ENABLE_LOCKS
    if (call->error) {
	return 0;
    }
#endif /* RX_ENABLE_LOCKS */

    call->iov = NULL;
    *nio = call->iovNext;
    return nbytes - call->iovNBytes;
}

int
rx_ReadvProc(struct rx_call *call, struct iovec *iov, int *nio, int maxio,
	     int nbytes)
{
    int bytes;
    SPLVAR;

    NETPRI;
    MUTEX_ENTER(&call->lock);
    bytes = rxi_ReadvProc(call, iov, nio, maxio, nbytes);
    MUTEX_EXIT(&call->lock);
    USERPRI;
    return bytes;
}

/* rxi_WriteProc -- internal version.
 *
 * LOCKS USED -- called at netpri with rx global lock and call->lock held. */

int
rxi_WriteProc(register struct rx_call *call, register char *buf,
	      register int nbytes)
{
    struct rx_connection *conn = call->conn;
    register struct rx_packet *cp = call->currentPacket;
    register unsigned int t;
    int requestCount = nbytes;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (queue_IsNotEmpty(&call->iovq)) {
	rxi_FreePackets(0, &call->iovq);
    }

    if (call->mode != RX_MODE_SENDING) {
	if ((conn->type == RX_SERVER_CONNECTION)
	    && (call->mode == RX_MODE_RECEIVING)) {
	    call->mode = RX_MODE_SENDING;
	    if (cp) {
		rxi_FreePacket(cp);
		cp = call->currentPacket = (struct rx_packet *)0;
		call->nLeft = 0;
		call->nFree = 0;
	    }
	} else {
	    return 0;
	}
    }

    /* Loop condition is checked at end, so that a write of 0 bytes
     * will force a packet to be created--specially for the case where
     * there are 0 bytes on the stream, but we must send a packet
     * anyway. */
    do {
	if (call->nFree == 0) {
	    if (!call->error && cp) {
#ifdef AFS_GLOBAL_RXLOCK_KERNEL
		/* Wait until TQ_BUSY is reset before adding any
		 * packets to the transmit queue
		 */
		while (call->flags & RX_CALL_TQ_BUSY) {
		    call->flags |= RX_CALL_TQ_WAIT;
#ifdef RX_ENABLE_LOCKS
		    CV_WAIT(&call->cv_tq, &call->lock);
#else /* RX_ENABLE_LOCKS */
		    osi_rxSleep(&call->tq);
#endif /* RX_ENABLE_LOCKS */
		}
#endif /* AFS_GLOBAL_RXLOCK_KERNEL */
		clock_NewTime();	/* Bogus:  need new time package */
		/* The 0, below, specifies that it is not the last packet: 
		 * there will be others. PrepareSendPacket may
		 * alter the packet length by up to
		 * conn->securityMaxTrailerSize */
		hadd32(call->bytesSent, cp->length);
		rxi_PrepareSendPacket(call, cp, 0);
		queue_Append(&call->tq, cp);
		cp = call->currentPacket = NULL;
		if (!
		    (call->
		     flags & (RX_CALL_FAST_RECOVER |
			      RX_CALL_FAST_RECOVER_WAIT))) {
		    rxi_Start(0, call, 0, 0);
		}
	    }
	    /* Wait for transmit window to open up */
	    while (!call->error
		   && call->tnext + 1 > call->tfirst + call->twind) {
		clock_NewTime();
		call->startWait = clock_Sec();

#ifdef	RX_ENABLE_LOCKS
		CV_WAIT(&call->cv_twind, &call->lock);
#else
		call->flags |= RX_CALL_WAIT_WINDOW_ALLOC;
		osi_rxSleep(&call->twind);
#endif

		call->startWait = 0;
#ifdef RX_ENABLE_LOCKS
		if (call->error) {
		    return 0;
		}
#endif /* RX_ENABLE_LOCKS */
	    }
	    if ((cp = rxi_AllocSendPacket(call, nbytes))) {
		call->currentPacket = cp;
		call->nFree = cp->length;
		call->curvec = 1;	/* 0th vec is always header */
		/* begin at the beginning [ more or less ], continue 
		 * on until the end, then stop. */
		call->curpos =
		    (char *)cp->wirevec[1].iov_base +
		    call->conn->securityHeaderSize;
		call->curlen =
		    cp->wirevec[1].iov_len - call->conn->securityHeaderSize;
	    }
	    if (call->error) {
		if (cp) {
		    rxi_FreePacket(cp);
		    call->currentPacket = NULL;
		}
		return 0;
	    }
	}

	if (cp && (int)call->nFree < nbytes) {
	    /* Try to extend the current buffer */
	    register int len, mud;
	    len = cp->length;
	    mud = rx_MaxUserDataSize(call);
	    if (mud > len) {
		int want;
		want = MIN(nbytes - (int)call->nFree, mud - len);
		rxi_AllocDataBuf(cp, want, RX_PACKET_CLASS_SEND_CBUF);
		if (cp->length > (unsigned)mud)
		    cp->length = mud;
		call->nFree += (cp->length - len);
	    }
	}

	/* If the remaining bytes fit in the buffer, then store them
	 * and return.  Don't ship a buffer that's full immediately to
	 * the peer--we don't know if it's the last buffer yet */

	if (!cp) {
	    call->nFree = 0;
	}

	while (nbytes && call->nFree) {

	    t = MIN((int)call->curlen, nbytes);
	    t = MIN((int)call->nFree, t);
	    memcpy(call->curpos, buf, t);
	    buf += t;
	    nbytes -= t;
	    call->curpos += t;
	    call->curlen -= (u_short)t;
	    call->nFree -= (u_short)t;

	    if (!call->curlen) {
		/* need to get another struct iov */
		if (++call->curvec >= cp->niovecs) {
		    /* current packet is full, extend or send it */
		    call->nFree = 0;
		} else {
		    call->curpos = (char *)cp->wirevec[call->curvec].iov_base;
		    call->curlen = cp->wirevec[call->curvec].iov_len;
		}
	    }
	}			/* while bytes to send and room to send them */

	/* might be out of space now */
	if (!nbytes) {
	    return requestCount;
	} else;			/* more data to send, so get another packet and keep going */
    } while (nbytes);

    return requestCount - nbytes;
}

int
rx_WriteProc(struct rx_call *call, char *buf, int nbytes)
{
    int bytes;
    int tcurlen;
    int tnFree;
    char *tcurpos;
    SPLVAR;

    /*
     * Free any packets from the last call to ReadvProc/WritevProc.
     * We do not need the lock because the receiver threads only
     * touch the iovq when the RX_CALL_IOVEC_WAIT flag is set, and the
     * RX_CALL_IOVEC_WAIT is always cleared before returning from
     * ReadvProc/WritevProc.
     */
    if (queue_IsNotEmpty(&call->iovq)) {
	rxi_FreePackets(0, &call->iovq);
    }

    /*
     * Most common case: all of the data fits in the current iovec.
     * We do not need the lock because this is the only thread that
     * updates the curlen, curpos, nFree fields.
     *
     * We are relying on nFree being zero unless the call is in send mode.
     */
    tcurlen = (int)call->curlen;
    tnFree = (int)call->nFree;
    if (!call->error && tcurlen >= nbytes && tnFree >= nbytes) {
	tcurpos = call->curpos;
	memcpy(tcurpos, buf, nbytes);
	call->curpos = tcurpos + nbytes;
	call->curlen = (u_short)(tcurlen - nbytes);
	call->nFree = (u_short)(tnFree - nbytes);
	return nbytes;
    }

    NETPRI;
    MUTEX_ENTER(&call->lock);
    bytes = rxi_WriteProc(call, buf, nbytes);
    MUTEX_EXIT(&call->lock);
    USERPRI;
    return bytes;
}

/* Optimization for marshalling 32 bit arguments */
int
rx_WriteProc32(register struct rx_call *call, register afs_int32 * value)
{
    int bytes;
    int tcurlen;
    int tnFree;
    char *tcurpos;
    SPLVAR;

    /*
     * Free any packets from the last call to ReadvProc/WritevProc.
     * We do not need the lock because the receiver threads only
     * touch the iovq when the RX_CALL_IOVEC_WAIT flag is set, and the
     * RX_CALL_IOVEC_WAIT is always cleared before returning from
     * ReadvProc/WritevProc.
     */
    if (queue_IsNotEmpty(&call->iovq)) {
	rxi_FreePackets(0, &call->iovq);
    }

    /*
     * Most common case: all of the data fits in the current iovec.
     * We do not need the lock because this is the only thread that
     * updates the curlen, curpos, nFree fields.
     *
     * We are relying on nFree being zero unless the call is in send mode.
     */
    tcurlen = call->curlen;
    tnFree = call->nFree;
    if (!call->error && tcurlen >= sizeof(afs_int32)
	&& tnFree >= sizeof(afs_int32)) {
	tcurpos = call->curpos;
	if (!((size_t)tcurpos & (sizeof(afs_int32) - 1))) {
	    *((afs_int32 *) (tcurpos)) = *value;
	} else {
	    memcpy(tcurpos, (char *)value, sizeof(afs_int32));
	}
	call->curpos = tcurpos + sizeof(afs_int32);
	call->curlen = (u_short)(tcurlen - sizeof(afs_int32));
	call->nFree = (u_short)(tnFree - sizeof(afs_int32));
	return sizeof(afs_int32);
    }

    NETPRI;
    MUTEX_ENTER(&call->lock);
    bytes = rxi_WriteProc(call, (char *)value, sizeof(afs_int32));
    MUTEX_EXIT(&call->lock);
    USERPRI;
    return bytes;
}

/* rxi_WritevAlloc -- internal version.
 *
 * Fill in an iovec to point to data in packet buffers. The application
 * calls rxi_WritevProc when the buffers are full.
 *
 * LOCKS USED -- called at netpri with rx global lock and call->lock held. */

int
rxi_WritevAlloc(struct rx_call *call, struct iovec *iov, int *nio, int maxio,
		int nbytes)
{
    struct rx_connection *conn = call->conn;
    struct rx_packet *cp = call->currentPacket;
    int requestCount;
    int nextio;
    /* Temporary values, real work is done in rxi_WritevProc */
    int tnFree;
    int tcurvec;
    char *tcurpos;
    int tcurlen;

    requestCount = nbytes;
    nextio = 0;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (queue_IsNotEmpty(&call->iovq)) {
        rxi_FreePackets(0, &call->iovq);
    }

    if (call->mode != RX_MODE_SENDING) {
	if ((conn->type == RX_SERVER_CONNECTION)
	    && (call->mode == RX_MODE_RECEIVING)) {
	    call->mode = RX_MODE_SENDING;
	    if (cp) {
		rxi_FreePacket(cp);
		cp = call->currentPacket = (struct rx_packet *)0;
		call->nLeft = 0;
		call->nFree = 0;
	    }
	} else {
	    return 0;
	}
    }

    /* Set up the iovec to point to data in packet buffers. */
    tnFree = call->nFree;
    tcurvec = call->curvec;
    tcurpos = call->curpos;
    tcurlen = call->curlen;
    do {
	register unsigned int t;

	if (tnFree == 0) {
	    /* current packet is full, allocate a new one */
	    cp = rxi_AllocSendPacket(call, nbytes);
	    if (cp == NULL) {
		/* out of space, return what we have */
		*nio = nextio;
		return requestCount - nbytes;
	    }
	    queue_Append(&call->iovq, cp);
	    tnFree = cp->length;
	    tcurvec = 1;
	    tcurpos =
		(char *)cp->wirevec[1].iov_base +
		call->conn->securityHeaderSize;
	    tcurlen = cp->wirevec[1].iov_len - call->conn->securityHeaderSize;
	}

	if (tnFree < nbytes) {
	    /* try to extend the current packet */
	    register int len, mud;
	    len = cp->length;
	    mud = rx_MaxUserDataSize(call);
	    if (mud > len) {
		int want;
		want = MIN(nbytes - tnFree, mud - len);
		rxi_AllocDataBuf(cp, want, RX_PACKET_CLASS_SEND_CBUF);
		if (cp->length > (unsigned)mud)
		    cp->length = mud;
		tnFree += (cp->length - len);
		if (cp == call->currentPacket) {
		    call->nFree += (cp->length - len);
		}
	    }
	}

	/* fill in the next entry in the iovec */
	t = MIN(tcurlen, nbytes);
	t = MIN(tnFree, t);
	iov[nextio].iov_base = tcurpos;
	iov[nextio].iov_len = t;
	nbytes -= t;
	tcurpos += t;
	tcurlen -= t;
	tnFree -= t;
	nextio++;

	if (!tcurlen) {
	    /* need to get another struct iov */
	    if (++tcurvec >= cp->niovecs) {
		/* current packet is full, extend it or move on to next packet */
		tnFree = 0;
	    } else {
		tcurpos = (char *)cp->wirevec[tcurvec].iov_base;
		tcurlen = cp->wirevec[tcurvec].iov_len;
	    }
	}
    } while (nbytes && nextio < maxio);
    *nio = nextio;
    return requestCount - nbytes;
}

int
rx_WritevAlloc(struct rx_call *call, struct iovec *iov, int *nio, int maxio,
	       int nbytes)
{
    int bytes;
    SPLVAR;

    NETPRI;
    MUTEX_ENTER(&call->lock);
    bytes = rxi_WritevAlloc(call, iov, nio, maxio, nbytes);
    MUTEX_EXIT(&call->lock);
    USERPRI;
    return bytes;
}

/* rxi_WritevProc -- internal version.
 *
 * Send buffers allocated in rxi_WritevAlloc.
 *
 * LOCKS USED -- called at netpri with rx global lock and call->lock held. */

int
rxi_WritevProc(struct rx_call *call, struct iovec *iov, int nio, int nbytes)
{
    struct rx_packet *cp = call->currentPacket;
    int nextio;
    int requestCount;
    struct rx_queue tmpq;

    requestCount = nbytes;
    nextio = 0;

    if (call->mode != RX_MODE_SENDING) {
	call->error = RX_PROTOCOL_ERROR;
    }
#ifdef AFS_GLOBAL_RXLOCK_KERNEL
    /* Wait until TQ_BUSY is reset before trying to move any
     * packets to the transmit queue.  */
    while (!call->error && call->flags & RX_CALL_TQ_BUSY) {
	call->flags |= RX_CALL_TQ_WAIT;
#ifdef RX_ENABLE_LOCKS
	CV_WAIT(&call->cv_tq, &call->lock);
#else /* RX_ENABLE_LOCKS */
	osi_rxSleep(&call->tq);
#endif /* RX_ENABLE_LOCKS */
    }
#endif /* AFS_GLOBAL_RXLOCK_KERNEL */

    if (call->error) {
	if (cp) {
	    queue_Prepend(&call->iovq, cp);
	    cp = call->currentPacket = NULL;
	}
	rxi_FreePackets(0, &call->iovq);
	return 0;
    }

    /* Loop through the I/O vector adjusting packet pointers.
     * Place full packets back onto the iovq once they are ready
     * to send. Set RX_PROTOCOL_ERROR if any problems are found in
     * the iovec. We put the loop condition at the end to ensure that
     * a zero length write will push a short packet. */
    nextio = 0;
    queue_Init(&tmpq);
    do {
	if (call->nFree == 0 && cp) {
	    clock_NewTime();	/* Bogus:  need new time package */
	    /* The 0, below, specifies that it is not the last packet: 
	     * there will be others. PrepareSendPacket may
	     * alter the packet length by up to
	     * conn->securityMaxTrailerSize */
	    hadd32(call->bytesSent, cp->length);
	    rxi_PrepareSendPacket(call, cp, 0);
	    queue_Append(&tmpq, cp);

	    /* The head of the iovq is now the current packet */
	    if (nbytes) {
		if (queue_IsEmpty(&call->iovq)) {
		    call->error = RX_PROTOCOL_ERROR;
		    cp = call->currentPacket = NULL;
		    rxi_FreePackets(0, &tmpq);
		    return 0;
		}
		cp = queue_First(&call->iovq, rx_packet);
		queue_Remove(cp);
		call->currentPacket = cp;
		call->nFree = cp->length;
		call->curvec = 1;
		call->curpos =
		    (char *)cp->wirevec[1].iov_base +
		    call->conn->securityHeaderSize;
		call->curlen =
		    cp->wirevec[1].iov_len - call->conn->securityHeaderSize;
	    }
	}

	if (nbytes) {
	    /* The next iovec should point to the current position */
	    if (iov[nextio].iov_base != call->curpos
		|| iov[nextio].iov_len > (int)call->curlen) {
		call->error = RX_PROTOCOL_ERROR;
		if (cp) {
		    queue_Prepend(&tmpq, cp);
		    call->currentPacket = NULL;
		}
		rxi_FreePackets(0, &tmpq);
		return 0;
	    }
	    nbytes -= iov[nextio].iov_len;
	    call->curpos += iov[nextio].iov_len;
	    call->curlen -= iov[nextio].iov_len;
	    call->nFree -= iov[nextio].iov_len;
	    nextio++;
	    if (call->curlen == 0) {
		if (++call->curvec > cp->niovecs) {
		    call->nFree = 0;
		} else {
		    call->curpos = (char *)cp->wirevec[call->curvec].iov_base;
		    call->curlen = cp->wirevec[call->curvec].iov_len;
		}
	    }
	}
    } while (nbytes && nextio < nio);

    /* Move the packets from the temporary queue onto the transmit queue.
     * We may end up with more than call->twind packets on the queue. */
    queue_SpliceAppend(&call->tq, &tmpq);

    if (!(call->flags & (RX_CALL_FAST_RECOVER | RX_CALL_FAST_RECOVER_WAIT))) {
	rxi_Start(0, call, 0, 0);
    }

    /* Wait for the length of the transmit queue to fall below call->twind */
    while (!call->error && call->tnext + 1 > call->tfirst + call->twind) {
	clock_NewTime();
	call->startWait = clock_Sec();
#ifdef	RX_ENABLE_LOCKS
	CV_WAIT(&call->cv_twind, &call->lock);
#else
	call->flags |= RX_CALL_WAIT_WINDOW_ALLOC;
	osi_rxSleep(&call->twind);
#endif
	call->startWait = 0;
    }

    if (call->error) {
	if (cp) {
	    rxi_FreePacket(cp);
	    cp = call->currentPacket = NULL;
	}
	return 0;
    }

    return requestCount - nbytes;
}

int
rx_WritevProc(struct rx_call *call, struct iovec *iov, int nio, int nbytes)
{
    int bytes;
    SPLVAR;

    NETPRI;
    MUTEX_ENTER(&call->lock);
    bytes = rxi_WritevProc(call, iov, nio, nbytes);
    MUTEX_EXIT(&call->lock);
    USERPRI;
    return bytes;
}

/* Flush any buffered data to the stream, switch to read mode
 * (clients) or to EOF mode (servers) */
void
rxi_FlushWrite(register struct rx_call *call)
{
    register struct rx_packet *cp = call->currentPacket;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (queue_IsNotEmpty(&call->iovq)) {
	rxi_FreePackets(0, &call->iovq);
    }

    if (call->mode == RX_MODE_SENDING) {

	call->mode =
	    (call->conn->type ==
	     RX_CLIENT_CONNECTION ? RX_MODE_RECEIVING : RX_MODE_EOF);

#ifdef RX_KERNEL_TRACE
	{
	    int glockOwner = ISAFS_GLOCK();
	    if (!glockOwner)
		AFS_GLOCK();
	    afs_Trace3(afs_iclSetp, CM_TRACE_WASHERE, ICL_TYPE_STRING,
		       __FILE__, ICL_TYPE_INT32, __LINE__, ICL_TYPE_POINTER,
		       call);
	    if (!glockOwner)
		AFS_GUNLOCK();
	}
#endif

#ifdef AFS_GLOBAL_RXLOCK_KERNEL
	/* Wait until TQ_BUSY is reset before adding any
	 * packets to the transmit queue
	 */
	while (call->flags & RX_CALL_TQ_BUSY) {
	    call->flags |= RX_CALL_TQ_WAIT;
#ifdef RX_ENABLE_LOCKS
	    CV_WAIT(&call->cv_tq, &call->lock);
#else /* RX_ENABLE_LOCKS */
	    osi_rxSleep(&call->tq);
#endif /* RX_ENABLE_LOCKS */
	}
#endif /* AFS_GLOBAL_RXLOCK_KERNEL */

	if (cp) {
	    /* cp->length is only supposed to be the user's data */
	    /* cp->length was already set to (then-current) 
	     * MaxUserDataSize or less. */
	    cp->length -= call->nFree;
	    call->currentPacket = (struct rx_packet *)0;
	    call->nFree = 0;
	} else {
	    cp = rxi_AllocSendPacket(call, 0);
	    if (!cp) {
		/* Mode can no longer be MODE_SENDING */
		return;
	    }
	    cp->length = 0;
	    cp->niovecs = 2;	/* header + space for rxkad stuff */
	    call->nFree = 0;
	}

	/* The 1 specifies that this is the last packet */
	hadd32(call->bytesSent, cp->length);
	rxi_PrepareSendPacket(call, cp, 1);
	queue_Append(&call->tq, cp);
	if (!
	    (call->
	     flags & (RX_CALL_FAST_RECOVER | RX_CALL_FAST_RECOVER_WAIT))) {
	    rxi_Start(0, call, 0, 0);
	}
    }
}

/* Flush any buffered data to the stream, switch to read mode
 * (clients) or to EOF mode (servers) */
void
rx_FlushWrite(struct rx_call *call)
{
    SPLVAR;
    NETPRI;
    MUTEX_ENTER(&call->lock);
    rxi_FlushWrite(call);
    MUTEX_EXIT(&call->lock);
    USERPRI;
}
