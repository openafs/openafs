/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006, Sine Nomine Associates
 */

/*
 * All Rights Reserved
 */
#ifndef _AFS_ICL_H__ENV_
#define _AFS_ICL_H__ENV_ 1

#ifdef	KERNEL
#include "param.h"
#include "afs_osi.h"
#include "lock.h"
#else
#include <lock.h>
typedef struct Lock afs_lock_t;
#endif

/*
 * NOTICE: the icl tracing subsystem has been
 * subsumed by osi_Trace
 *
 * all icl probe points are now osi_Trace probe points
 * please see /src/afs/afs_tracepoint.tab for the revised probe point namespace
 * please see /src/trace/event/icl.c for the new circular buffer encoding
 */

#include <osi/osi_includes.h>
#include <osi/osi_trace.h>
#include "afs/afs_icl.h"

#if !defined(OSI_TRACEPOINT_DISABLE)
#define afs_Trace_ProbeId(id) osi_Trace_icl_ProbeId(legacy_afs_trace_##id)
#define afs_Trace0(set, id) \
    osi_Trace_icl_Event(afs_Trace_ProbeId(id), osi_Trace_Args0())
#define afs_Trace1(set, id, t1, p1) \
    osi_Trace_icl_Event(afs_Trace_ProbeId(id), osi_Trace_Args2(t1, p1))
#define afs_Trace2(set, id, t1, p1, t2, p2) \
    osi_Trace_icl_Event(afs_Trace_ProbeId(id), \
		        osi_Trace_Args3(((t1) | ((t2) << 6)), p1, p2))
#define afs_Trace3(set, id, t1, p1, t2, p2, t3, p3) \
    osi_Trace_icl_Event(afs_Trace_ProbeId(id), \
		        osi_Trace_Args4(((t1) | ((t2) << 6) | ((t3) << 12)), p1, p2, p3))
#define afs_Trace4(set, id, t1, p1, t2, p2, t3, p3, t4, p4) \
    osi_Trace_icl_Event(afs_Trace_ProbeId(id), \
		        osi_Trace_Args5(((t1) | ((t2) << 6) | ((t3) << 12) | ((t4) << 18)), p1, p2, p3, p4))
#else /* OSI_TRACEPOINT_DISABLE */
#define afs_Trace0(set, id)
#define afs_Trace1(set, id, t1, p1)
#define afs_Trace2(set, id, t1, p1, t2, p2)
#define afs_Trace3(set, id, t1, p1, t2, p2, t3, p3)
#define afs_Trace4(set, id, t1, p1, t2, p2, t3, p3, t4, p4)
#endif /* OSI_TRACEPOINT_DISABLE */

/* types for icl_trace macros; values must be less than 64 */
#define ICL_TYPE_NONE		0	/* parameter doesn't exist */
#define ICL_TYPE_LONG		1
#define ICL_TYPE_INT32		7
#define ICL_TYPE_POINTER	2
#define ICL_TYPE_HYPER		3
#define ICL_TYPE_STRING		4
#define ICL_TYPE_FID		5       /* DEPRECATED */
#define	ICL_TYPE_UNIXDATE	6
#define ICL_TYPE_INT64		8
#define ICL_TYPE_OS_FID         9
#define ICL_TYPE_VENUS_FID     10
#define ICL_TYPE_VICE_FID      11
#define ICL_TYPE_OFFSET        12

#define ICL_HANDLE_OFFSET(x)   (&x)

#endif /* _AFS_ICL_H__ENV_ */
