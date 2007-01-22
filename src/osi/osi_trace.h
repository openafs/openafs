/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_TRACE_H
#define _OSI_OSI_TRACE_H 1


/*
 * osi tracing framework
 */


/* pull in necessary osi deps */
#include <osi/osi_mutex.h>
#include <osi/osi_mem.h>
#include <osi/osi_thread.h>
#include <osi/osi_cpu.h>

/* pull in main tracing framework headers */
#include <trace/buildenv.h>
#include <trace/types.h>
#include <trace/generator/tracepoint_impl.h>
#if defined(OSI_TRACE_CONSUMER_ENV)
#include <trace/consumer/activation.h>
#else /* !OSI_TRACE_CONSUMER_ENV */
#include <trace/generator/activation.h>
#include <trace/generator/activation_impl.h>
#endif /* !OSI_TRACE_CONSUMER_ENV */
#include <trace/event/event_prototypes.h>
#include <trace/common/module.h>
#include <trace/generator/tracepoint.h>


#if defined(OSI_TRACE_ENABLED)
/*
 * main initialization and shutdown routines
 * for the tracing framework
 *
 * these APIs should not be called by external 
 * end-user code.  osi_Init and osi_Shutdown
 * will call them for you
 */
osi_extern osi_result osi_Trace_PkgInit(void);
osi_extern osi_result osi_Trace_PkgShutdown(void);
#if defined(OSI_USERSPACE_ENV)
osi_extern osi_result osi_Trace_PkgChildInit(void);
#endif
#else /* !OSI_TRACE_ENABLED */
#define osi_Trace_PkgInit()       (OSI_OK)
#define osi_Trace_PkgShutdown()   (OSI_OK)
#define osi_Trace_PkgChildInit()  (OSI_OK)
#endif /* !OSI_TRACE_ENABLED */

#endif /* _OSI_OSI_TRACE_H */
