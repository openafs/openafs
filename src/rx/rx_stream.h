/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* rx_stream.h:  the stream I/O layer for RX */

This file is now obsolete.



#ifndef _RX_STREAM_
#define _RX_STREAM_

#ifdef	KERNEL
#include "../rx/rx.h"
#else /* KERNEL */
#include <sys/types.h>
#include <sys/uio.h>
#include "rx.h"
#endif /* KERNEL */

/* Write descriptor */
struct rx_stream_wd {
    char *freePtr;		/* Pointer to bytes in first packet */
    int	nFree;			/* Number of bytes free in first packet */
    struct rx_call *call;	/* The call this stream is attached to */
    struct rx_queue wq;		/* Currently allocated packets for this stream */
    int	packetSize;		/* Data size used in each packet */
};

/* Read descriptor */
struct rx_stream_rd {
    struct rx_packet *packet;	/* The current packet */
    char *nextByte;		/* Pointer to bytes in current packet */
    int	nLeft;			/* Number of bytes free in current packet */
    struct rx_call *call;	/* The call this stream is attached to */
    struct rx_queue rq;		/* Currently allocated packets for this stream */
    struct rx_queue freeTheseQ;	/* These packets should be freed on the next operation */
};

/* Composite stream descriptor */
struct rx_stream {
    union {
	struct rx_stream_rd rd;
	struct rx_stream_wd wd;
    } sd;
};

/* Externals */
void rx_stream_InitWrite();
void rx_stream_InitRead();
void rx_stream_FinishWrite();
void rx_stream_FinishRead();
int rx_stream_Read();
int rx_stream_Write();
int rx_stream_AllocIov();

/* Write nbytes of data to the write stream.  Returns the number of bytes written */
/* If it returns 0, the call status should be checked with rx_Error. */
#define	rx_stream_Write(iod, buf, nbytes)				\
    (iod)->sd.wd.nFree > (nbytes) ?					\
	(buf) && bcopy((buf), (iod)->sd.wd.freePtr, (nbytes)),		\
	(iod)->sd.wd.nFree -= (nbytes),					\
	(iod)->sd.wd.freePtr += (nbytes), (nbytes)			\
      : rx_stream_WriteProc((iod), (buf), (nbytes))


/* Read nbytes of data from the read stream.  Returns the number of bytes read */
/* If it returns less than requested, the call status should be checked with rx_Error */
#define	rx_stream_Read(iod, buf, nbytes)					\
    (iod)->sd.rd.nLeft > (nbytes) ?						\
    bcopy((iod)->sd.rd.nextByte, (buf), (nbytes)),				\
    (iod)->sd.rd.nLeft -= (nbytes), (iod)->sd.rd.nextByte += (nbytes), (nbytes)	\
   : rx_stream_ReadProc((iod), (buf), (nbytes))

#endif /* _RX_STREAM_	 End of rx_stream.h */
