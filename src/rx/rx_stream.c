
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

/* RX stream:  the stream I/O layer for RX */

This file is now obsolete.

#ifdef	KERNEL
#include "../h/types.h"
#include "../h/uio.h"
#include "../rx/rx_stream.h"
#else /* KERNEL */
#include "rx_stream.h"
#endif /* KERNEL */

void rx_stream_InitRead(sd, call)
    struct rx_stream *sd;
    struct rx_call *call;
{
    queue_Init(&sd->sd.rd.rq);
    queue_Init(&sd->sd.rd.freeTheseQ);
    sd->sd.rd.nLeft = 0;
    sd->sd.rd.call = call;
}

/* Normally called from the macro, rx_stream_Read */
int rx_stream_ReadProc(sd, buf, nbytes)
    struct rx_stream *sd;
    char *buf;
    int nbytes;
{
    int totalBytes = nbytes;

    if (queue_IsNotEmpty(&sd->sd.rd.freeTheseQ)) rx_FreePackets(&sd->sd.rd.freeTheseQ);

    while (nbytes) {
	struct rx_packet *tp;
	if (queue_IsEmpty(&sd->sd.rd.rq)) {
	    if (rx_ReadData(sd->sd.rd.call, &sd->sd.rd.rq)) return totalBytes - nbytes;
	    tp = queue_First(&sd->sd.rd.rq, rx_packet);
	    sd->sd.rd.nextByte = rx_DataOf(tp);
	    sd->sd.rd.nLeft = rx_GetDataSize(tp);
	}
	if (nbytes < sd->sd.rd.nLeft) {
	    sd->sd.rd.nLeft -= nbytes;
	    bcopy(sd->sd.rd.nextByte, buf, nbytes);
	    sd->sd.rd.nextByte += nbytes;
	    return totalBytes;
	}
	bcopy(sd->sd.rd.nextByte, buf, sd->sd.rd.nLeft);
	buf += sd->sd.rd.nLeft;
	nbytes -= sd->sd.rd.nLeft;
	tp = queue_First(&sd->sd.rd.rq, rx_packet);
	queue_Remove(tp);
	rx_FreePacket(tp);
	if (queue_IsNotEmpty(&sd->sd.rd.rq)) {
	    tp = queue_First(&sd->sd.rd.rq, rx_packet);
	    sd->sd.rd.nextByte = rx_DataOf(tp);
	    sd->sd.rd.nLeft = rx_GetDataSize(tp);
	}
    }
    return totalBytes;
}

int rx_stream_ReadIov(sd, iovlenp, iov, nbytes)
    struct rx_stream *sd;
    int *iovlenp;
    struct iovec *iov;
    int nbytes;
{
    int totalBytes = nbytes;
    int iovIndex = 0;
    int maxiovlen = *iovlenp;

    if (queue_IsNotEmpty(&sd->sd.rd.freeTheseQ)) rx_FreePackets(&sd->sd.rd.freeTheseQ);

    while (nbytes && iovIndex < maxiovlen) {
	struct rx_packet *tp;
	if (queue_IsEmpty(&sd->sd.rd.rq)) {
	    if (rx_ReadData(sd->sd.rd.call, &sd->sd.rd.rq)) break;
	    tp = queue_First(&sd->sd.rd.rq, rx_packet);
	    sd->sd.rd.nextByte = rx_DataOf(tp);
	    sd->sd.rd.nLeft = rx_GetDataSize(tp);
	}
	if (nbytes < sd->sd.rd.nLeft) {
	    sd->sd.rd.nLeft -= nbytes;
	    iov[iovIndex].iov_base = sd->sd.rd.nextByte;
	    iov[iovIndex++].iov_len = nbytes;
	    sd->sd.rd.nextByte += nbytes;
	    nbytes = 0;
	    break;
	}
	iov[iovIndex].iov_base = sd->sd.rd.nextByte;
	iov[iovIndex++].iov_len = sd->sd.rd.nLeft;
	nbytes -= sd->sd.rd.nLeft;
	tp = queue_First(&sd->sd.rd.rq, rx_packet);
	queue_MovePrepend(&sd->sd.rd.freeTheseQ, tp);
	if (queue_IsNotEmpty(&sd->sd.rd.rq)) {
	    tp = queue_First(&sd->sd.rd.rq, rx_packet);
	    sd->sd.rd.nextByte = rx_DataOf(tp);
	    sd->sd.rd.nLeft = rx_GetDataSize(tp);
	}
    }
    *iovlenp = iovIndex;
    return totalBytes - nbytes;
}

void rx_stream_FinishRead(sd)
    struct rx_stream *sd;
{
    if (queue_IsNotEmpty(&sd->sd.rd.rq)) rx_FreePackets(&sd->sd.rd.rq);
    if (queue_IsNotEmpty(&sd->sd.rd.freeTheseQ)) rx_FreePackets(&sd->sd.rd.freeTheseQ);
}

void rx_stream_InitWrite(sd, call)
    struct rx_stream *sd;
    struct rx_call *call;
{
    sd->sd.wd.freePtr = 0;
    sd->sd.wd.nFree = 0;
    sd->sd.wd.call = call;
    queue_Init(&sd->sd.wd.wq);
    sd->sd.wd.packetSize = rx_MaxDataSize(rx_ConnectionOf(call));
}

/* The real write procedure (rx_stream_Write is a macro) */
int rx_stream_WriteProc(sd, buf, nbytes)
    struct rx_stream *sd;
    char *buf;
    int nbytes;
{
    int totalBytes = nbytes;
    while (nbytes) {
	if (queue_IsEmpty(&sd->sd.wd.wq)) {
	    if (rx_AllocPackets(sd->sd.wd.call, &sd->sd.wd.wq, 1, RX_WAIT)) break;
	    sd->sd.wd.nFree = sd->sd.wd.packetSize;
	    sd->sd.wd.freePtr = rx_DataOf(queue_First(&sd->sd.wd.wq, rx_packet));
	}
	if (nbytes < sd->sd.wd.nFree) {
	    if (buf) bcopy(buf, sd->sd.wd.freePtr, nbytes), buf += nbytes;
	    sd->sd.wd.nFree -= nbytes;
	    sd->sd.wd.freePtr += nbytes;
	    nbytes = 0;
	    break;
	}
	if (buf) bcopy(buf, sd->sd.wd.freePtr, sd->sd.wd.nFree), buf += sd->sd.wd.nFree;
	nbytes -= sd->sd.wd.nFree;	
	sd->sd.wd.nFree = 0;
	if (rx_stream_FlushWrite(sd)) break;
    }
    return totalBytes - nbytes;
}

/* Returns nbytes allocated */
int rx_stream_AllocIov(sd, iovlenp, iovp, nbytes)
    struct rx_stream *sd;
    int *iovlenp;
    struct iovec *iovp;
    int nbytes;
{
    struct rx_packet *p, *nxp;
    int maxiovlen = *iovlenp;
    int iovIndex;
    int totalBytes;
    int nFree;
    int niovs;

    for (nFree = 0, queue_Scan(&sd->sd.wd.wq, p, nxp, rx_packet)) {
	nFree += sd->sd.wd.packetSize;
    }
    if (sd->sd.wd.nFree) nFree = nFree - sd->sd.wd.packetSize + sd->sd.wd.nFree;

    /* Allocate the number of bytes requested, or an even portion */
    for (totalBytes = nbytes; ; totalBytes >>= 1) {
	/* Compute number of additional buffers, beyond current partial buffer, required */
	int nPackets;
	nbytes = totalBytes - nFree;
	if (nbytes < 0) {
	    niovs = 1;
	    break;
	}
	niovs = nPackets = (nbytes + sd->sd.wd.packetSize - 1)/sd->sd.wd.packetSize;
        if (nFree) niovs++;
	if (niovs <= maxiovlen) {
	    if (rx_AllocPackets(sd->sd.wd.call, &sd->sd.wd.wq, nPackets, 1)) break;
	    if (nFree == 0) {
		/* Since there weren't any packets allocated previously, setup new description for first packet */
		sd->sd.wd.nFree = sd->sd.wd.packetSize;
		sd->sd.wd.freePtr = rx_DataOf(queue_First(&sd->sd.wd.wq, rx_packet));
	    }
	    break;
	}	    
    }
   
    /* Create an iovec to describe the set of allocated	buffers */
    for (nFree = sd->sd.wd.nFree, nbytes = totalBytes, iovIndex = 0,
	  queue_Scan(&sd->sd.wd.wq, p, nxp, rx_packet),
	  iovIndex++, nFree = sd->sd.wd.packetSize) {
	if (iovIndex >= niovs) break;
	iovp[iovIndex].iov_base = rx_DataOf(p) + sd->sd.wd.packetSize - nFree;
	if (nbytes <= nFree) {
	    iovp[iovIndex].iov_len = nbytes;
	    nbytes = 0;
	    break;
	}
	nbytes -= nFree;
	iovp[iovIndex].iov_len = nFree;
    }
    *iovlenp = niovs;
    return totalBytes - nbytes;
}

/* Wrong, wrong, wrong */
rx_stream_FlushWrite(sd)
    struct rx_stream *sd;
{
    struct rx_queue q;
    queue_Init(&q);
    if (queue_IsNotEmpty(&sd->sd.wd.wq) && sd->sd.wd.nFree < sd->sd.wd.packetSize) {
	struct rx_packet *tp = queue_First(&sd->sd.wd.wq, rx_packet);
	queue_MoveAppend(&q, tp);
	rx_SetDataSize(queue_First(&q, rx_packet), sd->sd.wd.packetSize - sd->sd.wd.nFree);
	if (queue_IsNotEmpty(&sd->sd.wd.wq)) {
	    sd->sd.wd.nFree = sd->sd.wd.packetSize;
	    sd->sd.wd.freePtr = rx_DataOf(queue_First(&sd->sd.wd.wq, rx_packet));
	} else sd->sd.wd.nFree = 0;
	return (rx_SendData(sd->sd.wd.call, &q));
    }
    return 0;
}

void rx_stream_FinishWrite(sd)
    struct rx_stream *sd;
{
    rx_stream_FlushWrite(sd);
    if (queue_IsNotEmpty(&sd->sd.wd.wq)) rx_FreePackets(&sd->sd.wd.wq);
    sd->sd.wd.nFree = 0;
}
