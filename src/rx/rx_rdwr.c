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

#ifdef KERNEL
# ifndef UKERNEL
#  ifdef RX_KERNEL_TRACE
#   include "rx_kcommon.h"
#  endif
#  if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#   include "afs/sysincludes.h"
#  else
#   include "h/types.h"
#   include "h/time.h"
#   include "h/stat.h"
#   if defined(AFS_AIX_ENV) || defined(AFS_AUX_ENV) || defined(AFS_SUN5_ENV)
#    include "h/systm.h"
#   endif
#   ifdef AFS_LINUX_ENV
#    include "h/socket.h"
#   endif
#   include "netinet/in.h"
#   if defined(AFS_SGI_ENV)
#    include "afs/sysincludes.h"
#   endif
#  endif
#  include "afs/afs_args.h"
#  if	(defined(AFS_AUX_ENV) || defined(AFS_AIX_ENV))
#   include "h/systm.h"
#  endif
# else /* !UKERNEL */
#  include "afs/sysincludes.h"
# endif /* !UKERNEL */

# ifdef RXDEBUG
#  undef RXDEBUG			/* turn off debugging */
# endif /* RXDEBUG */

# include "afs/afs_osi.h"
# include "rx_kmutex.h"
# include "rx/rx_kernel.h"
# include "afs/lock.h"
#else /* KERNEL */
# include <roken.h>
# include <afs/opr.h>
#endif /* KERNEL */

#include "rx.h"
#include "rx_clock.h"
#include "rx_globals.h"
#include "rx_atomic.h"
#include "rx_internal.h"
#include "rx_conn.h"
#include "rx_call.h"
#include "rx_packet.h"

#ifdef RX_LOCKS_DB
/* rxdb_fileID is used to identify the lock location, along with line#. */
static int rxdb_fileID = RXDB_FILE_RX_RDWR;
#endif /* RX_LOCKS_DB */

/* Get the next packet in the receive queue
 *
 * Dispose of the call's currentPacket, and move the next packet in the
 * receive queue into the currentPacket field. If the next packet isn't
 * available, then currentPacket is left NULL.
 *
 * @param call
 * 	The RX call to manipulate
 * @returns
 * 	0 on success, an error code on failure
 *
 * @notes
 * 	Must be called with the call locked. Unlocks the call if returning
 * 	with an error.
 */

static int
rxi_GetNextPacket(struct rx_call *call) {
    struct rx_packet *rp;
    int error;

    if (call->app.currentPacket != NULL) {
#ifdef RX_TRACK_PACKETS
	call->app.currentPacket->flags |= RX_PKTFLAG_CP;
#endif
	rxi_FreePacket(call->app.currentPacket);
	call->app.currentPacket = NULL;
    }

    if (opr_queue_IsEmpty(&call->rq))
	return 0;

    /* Check that next packet available is next in sequence */
    rp = opr_queue_First(&call->rq, struct rx_packet, entry);
    if (rp->header.seq != call->rnext)
	return 0;

    opr_queue_Remove(&rp->entry);
#ifdef RX_TRACK_PACKETS
    rp->flags &= ~RX_PKTFLAG_RQ;
#endif
#ifdef RXDEBUG_PACKET
    call->rqc--;
#endif /* RXDEBUG_PACKET */

    /* RXS_CheckPacket called to undo RXS_PreparePacket's work.  It may
     * reduce the length of the packet by up to conn->maxTrailerSize,
     * to reflect the length of the data + the header. */
    if ((error = RXS_CheckPacket(call->conn->securityObject, call, rp))) {
	/* Used to merely shut down the call, but now we shut down the whole
	 * connection since this may indicate an attempt to hijack it */

	MUTEX_EXIT(&call->lock);
	rxi_ConnectionError(call->conn, error);
	MUTEX_ENTER(&call->conn->conn_data_lock);
	rp = rxi_SendConnectionAbort(call->conn, rp, 0, 0);
	MUTEX_EXIT(&call->conn->conn_data_lock);
	rxi_FreePacket(rp);

	return error;
     }

    call->rnext++;
    call->app.currentPacket = rp;
#ifdef RX_TRACK_PACKETS
    call->app.currentPacket->flags |= RX_PKTFLAG_CP;
#endif
    call->app.curvec = 1;	/* 0th vec is always header */

    /* begin at the beginning [ more or less ], continue on until the end,
     * then stop. */
    call->app.curpos = (char *)call->app.currentPacket->wirevec[1].iov_base +
		   call->conn->securityHeaderSize;
    call->app.curlen = call->app.currentPacket->wirevec[1].iov_len -
		   call->conn->securityHeaderSize;

    call->app.nLeft = call->app.currentPacket->length;
    call->app.bytesRcvd += call->app.currentPacket->length;

    call->nHardAcks++;

    return 0;
}

/* rxi_ReadProc -- internal version.
 *
 * LOCKS USED -- called at netpri
 */
int
rxi_ReadProc(struct rx_call *call, char *buf,
	     int nbytes)
{
    int requestCount;
    int code;
    unsigned int t;

/* XXXX took out clock_NewTime from here.  Was it needed? */
    requestCount = nbytes;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (!opr_queue_IsEmpty(&call->app.iovq)) {
#ifdef RXDEBUG_PACKET
        call->iovqc -=
#endif /* RXDEBUG_PACKET */
            rxi_FreePackets(0, &call->app.iovq);
    }

    do {
	if (call->app.nLeft == 0) {
	    /* Get next packet */
	    MUTEX_ENTER(&call->lock);
	    for (;;) {
		if (call->error || (call->app.mode != RX_MODE_RECEIVING)) {
		    if (call->error) {
                        call->app.mode = RX_MODE_ERROR;
			MUTEX_EXIT(&call->lock);
			return 0;
		    }
		    if (call->app.mode == RX_MODE_SENDING) {
			rxi_FlushWriteLocked(call);
			continue;
		    }
		}

		code = rxi_GetNextPacket(call);
		if (code)
		     return 0;

		if (call->app.currentPacket) {
		    if (!(call->flags & RX_CALL_RECEIVE_DONE)) {
			if (call->nHardAcks > (u_short) rxi_HardAckRate) {
			    rxi_CancelDelayedAckEvent(call);
			    rxi_SendAck(call, 0, 0, RX_ACK_DELAY, 0);
			} else {
			    /* Delay to consolidate ack packets */
			    rxi_PostDelayedAckEvent(call, &rx_hardAckDelay);
			}
		    }
		    break;
		}

                /*
                 * If we reach this point either we have no packets in the
                 * receive queue or the next packet in the queue is not the
                 * one we are looking for.  There is nothing else for us to
                 * do but wait for another packet to arrive.
                 */

		/* Are there ever going to be any more packets? */
		if (call->flags & RX_CALL_RECEIVE_DONE) {
		    MUTEX_EXIT(&call->lock);
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
		    MUTEX_EXIT(&call->lock);
		    return 0;
		}
#endif /* RX_ENABLE_LOCKS */
	    }
	    MUTEX_EXIT(&call->lock);
	} else
	    /* osi_Assert(cp); */
	    /* MTUXXX  this should be replaced by some error-recovery code before shipping */
	    /* yes, the following block is allowed to be the ELSE clause (or not) */
	    /* It's possible for call->app.nLeft to be smaller than any particular
	     * iov_len.  Usually, recvmsg doesn't change the iov_len, since it
	     * reflects the size of the buffer.  We have to keep track of the
	     * number of bytes read in the length field of the packet struct.  On
	     * the final portion of a received packet, it's almost certain that
	     * call->app.nLeft will be smaller than the final buffer. */
	    while (nbytes && call->app.currentPacket) {
		t = MIN((int)call->app.curlen, nbytes);
		t = MIN(t, (int)call->app.nLeft);
		memcpy(buf, call->app.curpos, t);
		buf += t;
		nbytes -= t;
		call->app.curpos += t;
		call->app.curlen -= t;
		call->app.nLeft -= t;

		if (!call->app.nLeft) {
		    /* out of packet.  Get another one. */
#ifdef RX_TRACK_PACKETS
		    call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
#endif
		    rxi_FreePacket(call->app.currentPacket);
		    call->app.currentPacket = NULL;
		} else if (!call->app.curlen) {
		    /* need to get another struct iov */
		    if (++call->app.curvec >= call->app.currentPacket->niovecs) {
			/* current packet is exhausted, get ready for another */
			/* don't worry about curvec and stuff, they get set somewhere else */
#ifdef RX_TRACK_PACKETS
			call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
#endif
			rxi_FreePacket(call->app.currentPacket);
			call->app.currentPacket = NULL;
			call->app.nLeft = 0;
		    } else {
			call->app.curpos =
			    call->app.currentPacket->wirevec[call->app.curvec].iov_base;
			call->app.curlen =
			    call->app.currentPacket->wirevec[call->app.curvec].iov_len;
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
    SPLVAR;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (!opr_queue_IsEmpty(&call->app.iovq)) {
#ifdef RXDEBUG_PACKET
        call->iovqc -=
#endif /* RXDEBUG_PACKET */
            rxi_FreePackets(0, &call->app.iovq);
    }

    /*
     * Most common case, all of the data is in the current iovec.
     * We are relying on nLeft being zero unless the call is in receive mode.
     */
    if (!call->error && call->app.curlen > nbytes && call->app.nLeft > nbytes) {
        memcpy(buf, call->app.curpos, nbytes);

	call->app.curpos += nbytes;
	call->app.curlen -= nbytes;
	call->app.nLeft  -= nbytes;

        if (!call->app.nLeft && call->app.currentPacket != NULL) {
            /* out of packet.  Get another one. */
            rxi_FreePacket(call->app.currentPacket);
            call->app.currentPacket = NULL;
        }
	return nbytes;
    }

    NETPRI;
    bytes = rxi_ReadProc(call, buf, nbytes);
    USERPRI;
    return bytes;
}

/* Optimization for unmarshalling 32 bit integers */
int
rx_ReadProc32(struct rx_call *call, afs_int32 * value)
{
    int bytes;
    SPLVAR;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (!opr_queue_IsEmpty(&call->app.iovq)) {
#ifdef RXDEBUG_PACKET
        call->iovqc -=
#endif /* RXDEBUG_PACKET */
            rxi_FreePackets(0, &call->app.iovq);
    }

    /*
     * Most common case, all of the data is in the current iovec.
     * We are relying on nLeft being zero unless the call is in receive mode.
     */
    if (!call->error && call->app.curlen >= sizeof(afs_int32)
	&& call->app.nLeft >= sizeof(afs_int32)) {

        memcpy((char *)value, call->app.curpos, sizeof(afs_int32));

        call->app.curpos += sizeof(afs_int32);
	call->app.curlen -= sizeof(afs_int32);
	call->app.nLeft  -= sizeof(afs_int32);

        if (!call->app.nLeft && call->app.currentPacket != NULL) {
            /* out of packet.  Get another one. */
            rxi_FreePacket(call->app.currentPacket);
            call->app.currentPacket = NULL;
        }
	return sizeof(afs_int32);
    }

    NETPRI;
    bytes = rxi_ReadProc(call, (char *)value, sizeof(afs_int32));
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
    int code;
    unsigned int t;
    struct iovec *call_iov;
    struct iovec *cur_iov = NULL;

    if (call->app.currentPacket) {
	cur_iov = &call->app.currentPacket->wirevec[call->app.curvec];
    }
    call_iov = &call->iov[call->iovNext];

    while (!call->error && call->iovNBytes && call->iovNext < call->iovMax) {
	if (call->app.nLeft == 0) {
	    /* Get next packet */
	    code = rxi_GetNextPacket(call);
	    if (code) {
		MUTEX_ENTER(&call->lock);
	        return 1;
	    }

	    if (call->app.currentPacket) {
		cur_iov = &call->app.currentPacket->wirevec[1];
		didConsume = 1;
		continue;
	    } else {
		break;
	    }
	}

	/* It's possible for call->app.nLeft to be smaller than any particular
	 * iov_len.  Usually, recvmsg doesn't change the iov_len, since it
	 * reflects the size of the buffer.  We have to keep track of the
	 * number of bytes read in the length field of the packet struct.  On
	 * the final portion of a received packet, it's almost certain that
	 * call->app.nLeft will be smaller than the final buffer. */
	while (call->iovNBytes
	       && call->iovNext < call->iovMax
	       && call->app.currentPacket) {

	    t = MIN((int)call->app.curlen, call->iovNBytes);
	    t = MIN(t, (int)call->app.nLeft);
	    call_iov->iov_base = call->app.curpos;
	    call_iov->iov_len = t;
	    call_iov++;
	    call->iovNext++;
	    call->iovNBytes -= t;
	    call->app.curpos += t;
	    call->app.curlen -= t;
	    call->app.nLeft -= t;

	    if (!call->app.nLeft) {
		/* out of packet.  Get another one. */
#ifdef RX_TRACK_PACKETS
                call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
                call->app.currentPacket->flags |= RX_PKTFLAG_IOVQ;
#endif
		opr_queue_Append(&call->app.iovq,
				 &call->app.currentPacket->entry);
#ifdef RXDEBUG_PACKET
                call->iovqc++;
#endif /* RXDEBUG_PACKET */
		call->app.currentPacket = NULL;
	    } else if (!call->app.curlen) {
		/* need to get another struct iov */
		if (++call->app.curvec >= call->app.currentPacket->niovecs) {
		    /* current packet is exhausted, get ready for another */
		    /* don't worry about curvec and stuff, they get set somewhere else */
#ifdef RX_TRACK_PACKETS
		    call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
		    call->app.currentPacket->flags |= RX_PKTFLAG_IOVQ;
#endif
		    opr_queue_Append(&call->app.iovq,
				     &call->app.currentPacket->entry);
#ifdef RXDEBUG_PACKET
                    call->iovqc++;
#endif /* RXDEBUG_PACKET */
		    call->app.currentPacket = NULL;
		    call->app.nLeft = 0;
		} else {
		    cur_iov++;
		    call->app.curpos = (char *)cur_iov->iov_base;
		    call->app.curlen = cur_iov->iov_len;
		}
	    }
	}
    }

    /* If we consumed any packets then check whether we need to
     * send a hard ack. */
    if (didConsume && (!(call->flags & RX_CALL_RECEIVE_DONE))) {
	if (call->nHardAcks > (u_short) rxi_HardAckRate) {
	    rxi_CancelDelayedAckEvent(call);
	    rxi_SendAck(call, 0, serial, RX_ACK_DELAY, 0);
	    didHardAck = 1;
	} else {
	    /* Delay to consolidate ack packets */
	    rxi_PostDelayedAckEvent(call, &rx_hardAckDelay);
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
 * LOCKS USED -- called at netpri.
 */
int
rxi_ReadvProc(struct rx_call *call, struct iovec *iov, int *nio, int maxio,
	      int nbytes)
{
    int bytes;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (!opr_queue_IsEmpty(&call->app.iovq)) {
#ifdef RXDEBUG_PACKET
        call->iovqc -=
#endif /* RXDEBUG_PACKET */
            rxi_FreePackets(0, &call->app.iovq);
    }

    if (call->app.mode == RX_MODE_SENDING) {
	rxi_FlushWrite(call);
    }

    MUTEX_ENTER(&call->lock);
    if (call->error)
        goto error;

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

    if (call->error)
        goto error;

    call->iov = NULL;
    *nio = call->iovNext;
    bytes = nbytes - call->iovNBytes;
    MUTEX_EXIT(&call->lock);
    return bytes;

  error:
    MUTEX_EXIT(&call->lock);
    call->app.mode = RX_MODE_ERROR;
    return 0;
}

int
rx_ReadvProc(struct rx_call *call, struct iovec *iov, int *nio, int maxio,
	     int nbytes)
{
    int bytes;
    SPLVAR;

    NETPRI;
    bytes = rxi_ReadvProc(call, iov, nio, maxio, nbytes);
    USERPRI;
    return bytes;
}

/* rxi_WriteProc -- internal version.
 *
 * LOCKS USED -- called at netpri
 */

int
rxi_WriteProc(struct rx_call *call, char *buf,
	      int nbytes)
{
    struct rx_connection *conn = call->conn;
    unsigned int t;
    int requestCount = nbytes;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (!opr_queue_IsEmpty(&call->app.iovq)) {
#ifdef RXDEBUG_PACKET
        call->iovqc -=
#endif /* RXDEBUG_PACKET */
            rxi_FreePackets(0, &call->app.iovq);
    }

    if (call->app.mode != RX_MODE_SENDING) {
	if ((conn->type == RX_SERVER_CONNECTION)
	    && (call->app.mode == RX_MODE_RECEIVING)) {
	    call->app.mode = RX_MODE_SENDING;
	    if (call->app.currentPacket) {
#ifdef RX_TRACK_PACKETS
		call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
#endif
		rxi_FreePacket(call->app.currentPacket);
		call->app.currentPacket = NULL;
		call->app.nLeft = 0;
		call->app.nFree = 0;
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
	if (call->app.nFree == 0) {
	    MUTEX_ENTER(&call->lock);
            if (call->error)
                call->app.mode = RX_MODE_ERROR;
	    if (!call->error && call->app.currentPacket) {
		clock_NewTime();	/* Bogus:  need new time package */
		/* The 0, below, specifies that it is not the last packet:
		 * there will be others. PrepareSendPacket may
		 * alter the packet length by up to
		 * conn->securityMaxTrailerSize */
		call->app.bytesSent += call->app.currentPacket->length;
		rxi_PrepareSendPacket(call, call->app.currentPacket, 0);
                /* PrepareSendPacket drops the call lock */
                rxi_WaitforTQBusy(call);
#ifdef RX_TRACK_PACKETS
		call->app.currentPacket->flags |= RX_PKTFLAG_TQ;
#endif
		opr_queue_Append(&call->tq,
				 &call->app.currentPacket->entry);
#ifdef RXDEBUG_PACKET
                call->tqc++;
#endif /* RXDEBUG_PACKET */
#ifdef RX_TRACK_PACKETS
                call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
#endif
                call->app.currentPacket = NULL;

		/* If the call is in recovery, let it exhaust its current
		 * retransmit queue before forcing it to send new packets
		 */
		if (!(call->flags & (RX_CALL_FAST_RECOVER))) {
		    rxi_Start(call, 0);
		}
	    } else if (call->app.currentPacket) {
#ifdef RX_TRACK_PACKETS
		call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
#endif
		rxi_FreePacket(call->app.currentPacket);
		call->app.currentPacket = NULL;
	    }
	    /* Wait for transmit window to open up */
	    while (!call->error
		   && call->tnext + 1 > call->tfirst + (2 * call->twind)) {
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
                    call->app.mode = RX_MODE_ERROR;
		    MUTEX_EXIT(&call->lock);
		    return 0;
		}
#endif /* RX_ENABLE_LOCKS */
	    }
	    if ((call->app.currentPacket = rxi_AllocSendPacket(call, nbytes))) {
#ifdef RX_TRACK_PACKETS
		call->app.currentPacket->flags |= RX_PKTFLAG_CP;
#endif
		call->app.nFree = call->app.currentPacket->length;
		call->app.curvec = 1;	/* 0th vec is always header */
		/* begin at the beginning [ more or less ], continue
		 * on until the end, then stop. */
		call->app.curpos =
		    (char *) call->app.currentPacket->wirevec[1].iov_base +
		    call->conn->securityHeaderSize;
		call->app.curlen =
		    call->app.currentPacket->wirevec[1].iov_len -
		    call->conn->securityHeaderSize;
	    }
	    if (call->error) {
                call->app.mode = RX_MODE_ERROR;
		if (call->app.currentPacket) {
#ifdef RX_TRACK_PACKETS
		    call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
#endif
		    rxi_FreePacket(call->app.currentPacket);
		    call->app.currentPacket = NULL;
		}
		MUTEX_EXIT(&call->lock);
		return 0;
	    }
	    MUTEX_EXIT(&call->lock);
	}

	if (call->app.currentPacket && (int)call->app.nFree < nbytes) {
	    /* Try to extend the current buffer */
	    int len, mud;
	    len = call->app.currentPacket->length;
	    mud = rx_MaxUserDataSize(call);
	    if (mud > len) {
		int want;
		want = MIN(nbytes - (int)call->app.nFree, mud - len);
		rxi_AllocDataBuf(call->app.currentPacket, want,
				 RX_PACKET_CLASS_SEND_CBUF);
		if (call->app.currentPacket->length > (unsigned)mud)
		    call->app.currentPacket->length = mud;
		call->app.nFree += (call->app.currentPacket->length - len);
	    }
	}

	/* If the remaining bytes fit in the buffer, then store them
	 * and return.  Don't ship a buffer that's full immediately to
	 * the peer--we don't know if it's the last buffer yet */

	if (!call->app.currentPacket) {
	    call->app.nFree = 0;
	}

	while (nbytes && call->app.nFree) {

	    t = MIN((int)call->app.curlen, nbytes);
	    t = MIN((int)call->app.nFree, t);
	    memcpy(call->app.curpos, buf, t);
	    buf += t;
	    nbytes -= t;
	    call->app.curpos += t;
	    call->app.curlen -= (u_short)t;
	    call->app.nFree -= (u_short)t;

	    if (!call->app.curlen) {
		/* need to get another struct iov */
		if (++call->app.curvec >= call->app.currentPacket->niovecs) {
		    /* current packet is full, extend or send it */
		    call->app.nFree = 0;
		} else {
		    call->app.curpos =
			call->app.currentPacket->wirevec[call->app.curvec].iov_base;
		    call->app.curlen =
			call->app.currentPacket->wirevec[call->app.curvec].iov_len;
		}
	    }
	}			/* while bytes to send and room to send them */

	/* might be out of space now */
	if (!nbytes) {
	    return requestCount;
	} else {
	    /* more data to send, so get another packet and keep going */
	}
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

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (!opr_queue_IsEmpty(&call->app.iovq)) {
#ifdef RXDEBUG_PACKET
        call->iovqc -=
#endif /* RXDEBUG_PACKET */
            rxi_FreePackets(0, &call->app.iovq);
    }

    /*
     * Most common case: all of the data fits in the current iovec.
     * We are relying on nFree being zero unless the call is in send mode.
     */
    tcurlen = (int)call->app.curlen;
    tnFree = (int)call->app.nFree;
    if (!call->error && tcurlen >= nbytes && tnFree >= nbytes) {
	tcurpos = call->app.curpos;

	memcpy(tcurpos, buf, nbytes);
	call->app.curpos = tcurpos + nbytes;
	call->app.curlen = (u_short)(tcurlen - nbytes);
	call->app.nFree = (u_short)(tnFree - nbytes);
	return nbytes;
    }

    NETPRI;
    bytes = rxi_WriteProc(call, buf, nbytes);
    USERPRI;
    return bytes;
}

/* Optimization for marshalling 32 bit arguments */
int
rx_WriteProc32(struct rx_call *call, afs_int32 * value)
{
    int bytes;
    int tcurlen;
    int tnFree;
    char *tcurpos;
    SPLVAR;

    if (!opr_queue_IsEmpty(&call->app.iovq)) {
#ifdef RXDEBUG_PACKET
        call->iovqc -=
#endif /* RXDEBUG_PACKET */
            rxi_FreePackets(0, &call->app.iovq);
    }

    /*
     * Most common case: all of the data fits in the current iovec.
     * We are relying on nFree being zero unless the call is in send mode.
     */
    tcurlen = call->app.curlen;
    tnFree = call->app.nFree;
    if (!call->error && tcurlen >= sizeof(afs_int32)
	&& tnFree >= sizeof(afs_int32)) {
	tcurpos = call->app.curpos;

	if (!((size_t)tcurpos & (sizeof(afs_int32) - 1))) {
	    *((afs_int32 *) (tcurpos)) = *value;
	} else {
	    memcpy(tcurpos, (char *)value, sizeof(afs_int32));
	}
	call->app.curpos = tcurpos + sizeof(afs_int32);
	call->app.curlen = (u_short)(tcurlen - sizeof(afs_int32));
	call->app.nFree = (u_short)(tnFree - sizeof(afs_int32));
	return sizeof(afs_int32);
    }

    NETPRI;
    bytes = rxi_WriteProc(call, (char *)value, sizeof(afs_int32));
    USERPRI;
    return bytes;
}

/* rxi_WritevAlloc -- internal version.
 *
 * Fill in an iovec to point to data in packet buffers. The application
 * calls rxi_WritevProc when the buffers are full.
 *
 * LOCKS USED -- called at netpri.
 */

static int
rxi_WritevAlloc(struct rx_call *call, struct iovec *iov, int *nio, int maxio,
		int nbytes)
{
    struct rx_connection *conn = call->conn;
    struct rx_packet *cp;
    int requestCount;
    int nextio;
    /* Temporary values, real work is done in rxi_WritevProc */
    int tnFree;
    unsigned int tcurvec;
    char *tcurpos;
    int tcurlen;

    requestCount = nbytes;
    nextio = 0;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (!opr_queue_IsEmpty(&call->app.iovq)) {
#ifdef RXDEBUG_PACKET
        call->iovqc -=
#endif /* RXDEBUG_PACKET */
            rxi_FreePackets(0, &call->app.iovq);
    }

    if (call->app.mode != RX_MODE_SENDING) {
	if ((conn->type == RX_SERVER_CONNECTION)
	    && (call->app.mode == RX_MODE_RECEIVING)) {
	    call->app.mode = RX_MODE_SENDING;
	    if (call->app.currentPacket) {
#ifdef RX_TRACK_PACKETS
		call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
#endif
		rxi_FreePacket(call->app.currentPacket);
		call->app.currentPacket = NULL;
		call->app.nLeft = 0;
		call->app.nFree = 0;
	    }
	} else {
	    return 0;
	}
    }

    /* Set up the iovec to point to data in packet buffers. */
    tnFree = call->app.nFree;
    tcurvec = call->app.curvec;
    tcurpos = call->app.curpos;
    tcurlen = call->app.curlen;
    cp = call->app.currentPacket;
    do {
	int t;

	if (tnFree == 0) {
	    /* current packet is full, allocate a new one */
	    MUTEX_ENTER(&call->lock);
	    cp = rxi_AllocSendPacket(call, nbytes);
	    MUTEX_EXIT(&call->lock);
	    if (cp == NULL) {
		/* out of space, return what we have */
		*nio = nextio;
		return requestCount - nbytes;
	    }
#ifdef RX_TRACK_PACKETS
	    cp->flags |= RX_PKTFLAG_IOVQ;
#endif
	    opr_queue_Append(&call->app.iovq, &cp->entry);
#ifdef RXDEBUG_PACKET
            call->iovqc++;
#endif /* RXDEBUG_PACKET */
	    tnFree = cp->length;
	    tcurvec = 1;
	    tcurpos =
		(char *)cp->wirevec[1].iov_base +
		call->conn->securityHeaderSize;
	    tcurlen = cp->wirevec[1].iov_len - call->conn->securityHeaderSize;
	}

	if (tnFree < nbytes) {
	    /* try to extend the current packet */
	    int len, mud;
	    len = cp->length;
	    mud = rx_MaxUserDataSize(call);
	    if (mud > len) {
		int want;
		want = MIN(nbytes - tnFree, mud - len);
		rxi_AllocDataBuf(cp, want, RX_PACKET_CLASS_SEND_CBUF);
		if (cp->length > (unsigned)mud)
		    cp->length = mud;
		tnFree += (cp->length - len);
		if (cp == call->app.currentPacket) {
		    call->app.nFree += (cp->length - len);
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
    bytes = rxi_WritevAlloc(call, iov, nio, maxio, nbytes);
    USERPRI;
    return bytes;
}

/* rxi_WritevProc -- internal version.
 *
 * Send buffers allocated in rxi_WritevAlloc.
 *
 * LOCKS USED -- called at netpri.
 */
int
rxi_WritevProc(struct rx_call *call, struct iovec *iov, int nio, int nbytes)
{
#ifdef RX_TRACK_PACKETS
    struct opr_queue *cursor;
#endif
    int nextio = 0;
    int requestCount;
    struct opr_queue tmpq;
#ifdef RXDEBUG_PACKET
    u_short tmpqc;
#endif

    requestCount = nbytes;

    MUTEX_ENTER(&call->lock);
    if (call->error) {
        call->app.mode = RX_MODE_ERROR;
    } else if (call->app.mode != RX_MODE_SENDING) {
	call->error = RX_PROTOCOL_ERROR;
    }
    rxi_WaitforTQBusy(call);

    if (call->error) {
        call->app.mode = RX_MODE_ERROR;
	MUTEX_EXIT(&call->lock);
	if (call->app.currentPacket) {
#ifdef RX_TRACK_PACKETS
            call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
            call->app.currentPacket->flags |= RX_PKTFLAG_IOVQ;
#endif
	    opr_queue_Prepend(&call->app.iovq,
			      &call->app.currentPacket->entry);
#ifdef RXDEBUG_PACKET
            call->iovqc++;
#endif /* RXDEBUG_PACKET */
	    call->app.currentPacket = NULL;
	}
#ifdef RXDEBUG_PACKET
        call->iovqc -=
#endif /* RXDEBUG_PACKET */
            rxi_FreePackets(0, &call->app.iovq);
	return 0;
    }

    /* Loop through the I/O vector adjusting packet pointers.
     * Place full packets back onto the iovq once they are ready
     * to send. Set RX_PROTOCOL_ERROR if any problems are found in
     * the iovec. We put the loop condition at the end to ensure that
     * a zero length write will push a short packet. */
    opr_queue_Init(&tmpq);
#ifdef RXDEBUG_PACKET
    tmpqc = 0;
#endif /* RXDEBUG_PACKET */
    do {
	if (call->app.nFree == 0 && call->app.currentPacket) {
	    clock_NewTime();	/* Bogus:  need new time package */
	    /* The 0, below, specifies that it is not the last packet:
	     * there will be others. PrepareSendPacket may
	     * alter the packet length by up to
	     * conn->securityMaxTrailerSize */
	    call->app.bytesSent += call->app.currentPacket->length;
	    rxi_PrepareSendPacket(call, call->app.currentPacket, 0);
            /* PrepareSendPacket drops the call lock */
            rxi_WaitforTQBusy(call);
	    opr_queue_Append(&tmpq, &call->app.currentPacket->entry);
#ifdef RXDEBUG_PACKET
            tmpqc++;
#endif /* RXDEBUG_PACKET */
            call->app.currentPacket = NULL;

	    /* The head of the iovq is now the current packet */
	    if (nbytes) {
		if (opr_queue_IsEmpty(&call->app.iovq)) {
                    MUTEX_EXIT(&call->lock);
		    call->error = RX_PROTOCOL_ERROR;
#ifdef RXDEBUG_PACKET
                    tmpqc -=
#endif /* RXDEBUG_PACKET */
                        rxi_FreePackets(0, &tmpq);
		    return 0;
		}
		call->app.currentPacket
			= opr_queue_First(&call->app.iovq, struct rx_packet,
					  entry);
		opr_queue_Remove(&call->app.currentPacket->entry);
#ifdef RX_TRACK_PACKETS
                call->app.currentPacket->flags &= ~RX_PKTFLAG_IOVQ;
		call->app.currentPacket->flags |= RX_PKTFLAG_CP;
#endif
#ifdef RXDEBUG_PACKET
                call->iovqc--;
#endif /* RXDEBUG_PACKET */
		call->app.nFree = call->app.currentPacket->length;
		call->app.curvec = 1;
		call->app.curpos =
		    (char *) call->app.currentPacket->wirevec[1].iov_base +
		    call->conn->securityHeaderSize;
		call->app.curlen =
		    call->app.currentPacket->wirevec[1].iov_len -
		    call->conn->securityHeaderSize;
	    }
	}

	if (nbytes) {
	    /* The next iovec should point to the current position */
	    if (iov[nextio].iov_base != call->app.curpos
		|| iov[nextio].iov_len > (int)call->app.curlen) {
		call->error = RX_PROTOCOL_ERROR;
                MUTEX_EXIT(&call->lock);
		if (call->app.currentPacket) {
#ifdef RX_TRACK_PACKETS
		    call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
#endif
                    opr_queue_Prepend(&tmpq,
				      &call->app.currentPacket->entry);
#ifdef RXDEBUG_PACKET
                    tmpqc++;
#endif /* RXDEBUG_PACKET */
                    call->app.currentPacket = NULL;
		}
#ifdef RXDEBUG_PACKET
                tmpqc -=
#endif /* RXDEBUG_PACKET */
                    rxi_FreePackets(0, &tmpq);
		return 0;
	    }
	    nbytes -= iov[nextio].iov_len;
	    call->app.curpos += iov[nextio].iov_len;
	    call->app.curlen -= iov[nextio].iov_len;
	    call->app.nFree -= iov[nextio].iov_len;
	    nextio++;
	    if (call->app.curlen == 0) {
		if (++call->app.curvec > call->app.currentPacket->niovecs) {
		    call->app.nFree = 0;
		} else {
		    call->app.curpos =
			call->app.currentPacket->wirevec[call->app.curvec].iov_base;
		    call->app.curlen =
			call->app.currentPacket->wirevec[call->app.curvec].iov_len;
		}
	    }
	}
    } while (nbytes && nextio < nio);

    /* Move the packets from the temporary queue onto the transmit queue.
     * We may end up with more than call->twind packets on the queue. */

#ifdef RX_TRACK_PACKETS
    for (opr_queue_Scan(&tmpq, cursor))
    {
	struct rx_packet *p = opr_queue_Entry(cursor, struct rx_packet, entry);
        p->flags |= RX_PKTFLAG_TQ;
    }
#endif
    if (call->error)
        call->app.mode = RX_MODE_ERROR;

    opr_queue_SpliceAppend(&call->tq, &tmpq);

    /* If the call is in recovery, let it exhaust its current retransmit
     * queue before forcing it to send new packets
     */
    if (!(call->flags & RX_CALL_FAST_RECOVER)) {
	rxi_Start(call, 0);
    }

    /* Wait for the length of the transmit queue to fall below call->twind */
    while (!call->error && call->tnext + 1 > call->tfirst + (2 * call->twind)) {
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
        call->app.mode = RX_MODE_ERROR;
        call->app.currentPacket = NULL;
        MUTEX_EXIT(&call->lock);
	if (call->app.currentPacket) {
#ifdef RX_TRACK_PACKETS
	    call->app.currentPacket->flags &= ~RX_PKTFLAG_CP;
#endif
	    rxi_FreePacket(call->app.currentPacket);
	}
	return 0;
    }
    MUTEX_EXIT(&call->lock);

    return requestCount - nbytes;
}

int
rx_WritevProc(struct rx_call *call, struct iovec *iov, int nio, int nbytes)
{
    int bytes;
    SPLVAR;

    NETPRI;
    bytes = rxi_WritevProc(call, iov, nio, nbytes);
    USERPRI;
    return bytes;
}

/* Flush any buffered data to the stream, switch to read mode
 * (clients) or to EOF mode (servers). If 'locked' is nonzero, call->lock must
 * be already held.
 *
 * LOCKS HELD: called at netpri.
 */
static void
FlushWrite(struct rx_call *call, int locked)
{
    struct rx_packet *cp = NULL;

    /* Free any packets from the last call to ReadvProc/WritevProc */
    if (!opr_queue_IsEmpty(&call->app.iovq)) {
#ifdef RXDEBUG_PACKET
        call->iovqc -=
#endif /* RXDEBUG_PACKET */
            rxi_FreePackets(0, &call->app.iovq);
    }

    if (call->app.mode == RX_MODE_SENDING) {

	call->app.mode =
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

        if (!locked) {
            MUTEX_ENTER(&call->lock);
        }

        if (call->error)
            call->app.mode = RX_MODE_ERROR;

	call->flags |= RX_CALL_FLUSH;

        cp = call->app.currentPacket;

	if (cp) {
	    /* cp->length is only supposed to be the user's data */
	    /* cp->length was already set to (then-current)
	     * MaxUserDataSize or less. */
#ifdef RX_TRACK_PACKETS
	    cp->flags &= ~RX_PKTFLAG_CP;
#endif
	    cp->length -= call->app.nFree;
	    call->app.currentPacket = NULL;
	    call->app.nFree = 0;
	} else {
	    cp = rxi_AllocSendPacket(call, 0);
	    if (!cp) {
		/* Mode can no longer be MODE_SENDING */
		return;
	    }
	    cp->length = 0;
	    cp->niovecs = 2;	/* header + space for rxkad stuff */
	    call->app.nFree = 0;
	}

	/* The 1 specifies that this is the last packet */
	call->app.bytesSent += cp->length;
	rxi_PrepareSendPacket(call, cp, 1);
        /* PrepareSendPacket drops the call lock */
        rxi_WaitforTQBusy(call);
#ifdef RX_TRACK_PACKETS
	cp->flags |= RX_PKTFLAG_TQ;
#endif
	opr_queue_Append(&call->tq, &cp->entry);
#ifdef RXDEBUG_PACKET
        call->tqc++;
#endif /* RXDEBUG_PACKET */

	/* If the call is in recovery, let it exhaust its current retransmit
	 * queue before forcing it to send new packets
	 */
	if (!(call->flags & RX_CALL_FAST_RECOVER)) {
	    rxi_Start(call, 0);
	}
        if (!locked) {
            MUTEX_EXIT(&call->lock);
        }
    }
}

void
rxi_FlushWrite(struct rx_call *call)
{
    FlushWrite(call, 0);
}

void
rxi_FlushWriteLocked(struct rx_call *call)
{
    FlushWrite(call, 1);
}

/* Flush any buffered data to the stream, switch to read mode
 * (clients) or to EOF mode (servers) */
void
rx_FlushWrite(struct rx_call *call)
{
    SPLVAR;
    NETPRI;
    FlushWrite(call, 0);
    USERPRI;
}
