/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_RX_MULTI_
#define _RX_MULTI_

struct multi_handle {
    int nConns;
    struct rx_call **calls;
    short *ready;
    short nReady;		/* XXX UNALIGNED */
    short *nextReady;
    short *firstNotReady;
#ifdef RX_ENABLE_LOCKS
    afs_kmutex_t lock;
    afs_kcondvar_t cv;
#endif				/* RX_ENABLE_LOCKS */
};

#define multi_Rx(conns, nConns) \
    do {\
	struct multi_handle *multi_h;\
	int multi_i;\
	int multi_i0;\
	afs_int32 multi_error;\
	struct rx_call *multi_call;\
	multi_h = multi_Init(conns, nConns);\
	for (multi_i0 = multi_i = 0; ; multi_i = multi_i0 )

#define multi_Body(startProc, endProc)\
	if (multi_h->nextReady == multi_h->firstNotReady && multi_i < multi_h->nConns) {\
	    multi_call = multi_h->calls[multi_i];\
            if (multi_call) {\
                startProc;\
	        rx_FlushWrite(multi_call);\
            }\
	    multi_i0++;  /* THIS is the loop variable!! */\
	    continue;\
	}\
	if ((multi_i = multi_Select(multi_h)) < 0) break;\
	multi_call = multi_h->calls[multi_i];\
	multi_error = rx_EndCall(multi_call, endProc);\
	multi_h->calls[multi_i] = (struct rx_call *) 0

#define	multi_Abort break

#define multi_End\
	multi_Finalize(multi_h);\
    } while (0)

/* Deprecated; use multi_End instead. */
#define multi_End_Ignore multi_End

#endif /* _RX_MULTI_     End of rx_multi.h */
