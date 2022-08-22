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

/*
 * It is generally recommended to keep the code in the body of a multi_Rx()
 * call as fast and simple as possible; that is, try not to call other RPCs or
 * obtain heavy locks.
 *
 * For example, do NOT do this:
 *
 * multi_Rx(conns, n_conns) {
 *     multi_FOO_Call(arg);
 *     if (multi_error == 0) {
 *         connSuccess = conns[multi_i];
 *         code = FOO_OtherCall(connSuccess);
 *         multi_Abort;
 *     }
 * } multi_End;
 *
 * If the body takes a long time to run (because, for example, we are waiting
 * for the net), we may be holding calls open for the other connections, even
 * if the peer has finished responding. This can prevent other threads from
 * using the connection if its call channels are full, causing unnecessary
 * delays. For complex applications, this can also make ordering rules for
 * locks and connections more complicated, making it more prone to deadlock
 * issues.
 *
 * Instead, do this:
 *
 * connSuccess = NULL;
 * multi_Rx(conns, n_conns) {
 *     multi_FOO_Call(arg);
 *     if (multi_error == 0) {
 *         connSuccess = conns[multi_i];
 *         multi_Abort;
 *     }
 * } multi_End;
 * if (connSuccess != NULL) {
 *     code = FOO_OtherCall(connSuccess);
 * }
 *
 * This allows all calls to complete and be released for reuse as soon as
 * possible.
 *
 * If you call multi_Rx on the same conns at the same time in multiple threads,
 * you must also provide a consistent ordering of the conns (for example, see
 * viced's usage of CompareCBA()). Otherwise, we can block on creating a call
 * on a conn, while another thread is waiting for our already-created call to
 * end, causing a deadlock.
 */
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

#endif /* _RX_MULTI_     End of rx_multi.h */
