/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifdef	VALIDATE
error - foo error - foo error - foo
#endif /* VALIDATE */
#ifndef RX_USER_INCLUDE
#define RX_USER_INCLUDE
/* rx_user.h:  definitions specific to the user-level implementation of Rx */
#include <afs/param.h>
#include <stdio.h>
#include <stdlib.h>		/* for malloc() */
#include <lwp.h>
/* These routines are no-ops in the user level implementation */
#define SPLVAR
#define NETPRI
#define USERPRI
#define AFS_GLOCK()
#define AFS_GUNLOCK()
#define AFS_ASSERT_GLOCK()
#ifndef UKERNEL
/* Defined in rx/UKERNEL/rx_kmutex.h */
#define ISAFS_GLOCK()
#endif
/* Some "operating-system independent" stuff, for the user mode implementation */
#ifdef UAFS_CLIENT
typedef void *osi_socket;
#define	OSI_NULLSOCKET	((osi_socket) 0)
#else /* UAFS_CLIENT */
#ifdef AFS_NT40_ENV
typedef SOCKET osi_socket;
#define OSI_NULLSOCKET INVALID_SOCKET
#else /* !AFS_NT40_ENV */
typedef afs_int32 osi_socket;
#define	OSI_NULLSOCKET	((osi_socket) -1)
#endif /* !AFS_NT40_ENV */
#endif /* UAFS_CLIENT */

#define	osi_rxSleep(x)		    rxi_Sleep(x)
#define	osi_rxWakeup(x)		    rxi_Wakeup(x)

#ifndef	AFS_AIX32_ENV

#ifndef osi_Alloc
#define	osi_Alloc(size)		    malloc(size)
#endif

#ifndef osi_Free
#define	osi_Free(ptr, size)	    free(ptr)
#endif

#endif

#define	osi_GetTime(timevalptr)	    gettimeofday(timevalptr, 0)

/* Just in case it's possible to distinguish between relatively long-lived stuff and stuff which will be freed very soon, but which needs quick allocation (e.g. dynamically allocated xdr things) */
#define	osi_QuickFree(ptr, size)    osi_Free(ptr, size)
#define	osi_QuickAlloc(size)	    osi_Alloc(size)

#define osi_Assert(e) (void)((e) || (osi_AssertFailU(#e, __FILE__, __LINE__), 0))

#define	osi_Msg			    fprintf)(stderr,

#endif /* RX_USER_INCLUDE */
