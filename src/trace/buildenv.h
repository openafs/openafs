/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_BUILDENV_H
#define _OSI_TRACE_BUILDENV_H 1


/*
 * osi tracing framework
 * build environment compat
 */

#include <osi/osi_mem.h>
#include <osi/osi_thread.h>


/* determine whether tracing framework should be enabled */
#if !defined(OSI_TRACE_DISABLE) && !defined(AFS_NT40_ENV)
#define OSI_TRACE_ENABLED 1
#else
#define OSI_TRACEPOINT_DISABLE 1
#endif


/*
 * trace storage methods
 */
#define OSI_TRACE_COMMIT_METHOD_NONE     16384
#define OSI_TRACE_COMMIT_METHOD_BUFFER   16385
#define OSI_TRACE_COMMIT_METHOD_SYSCALL  16386
#define OSI_TRACE_COMMIT_METHOD_IPC      16387

#define OSI_TRACE_COMMIT_METHOD_IS(x)  (x == OSI_TRACE_COMMIT_METHOD)

/* determine whether to store traces locally in a buffer, or to perform
 * some action when a trace commit is called */

#if !defined(OSI_TRACE_ENABLED)
#define OSI_TRACE_COMMIT_METHOD OSI_TRACE_COMMIT_METHOD_NONE
#elif defined(OSI_ENV_KERNELSPACE) || defined(OSI_TRACE_BUFFER_USERSPACE)
#define OSI_TRACE_COMMIT_METHOD OSI_TRACE_COMMIT_METHOD_BUFFER
#else
#define OSI_TRACE_COMMIT_METHOD OSI_TRACE_COMMIT_METHOD_SYSCALL
#endif

/* determine when context-local trace buffers should be enabled */
#if defined(OSI_IMPLEMENTS_MEM_LOCAL) && ((defined(OSI_ENV_KERNELSPACE) && defined(OSI_IMPLEMENTS_THREAD_BIND)) || defined(OSI_ENV_USERSPACE)) && !defined(OSI_TRACE_DISABLE_CTX_LOCAL_BUFFER)
#define OSI_TRACE_BUFFER_CTX_LOCAL 1
#endif



#endif /* _OSI_TRACE_BUILDENV_H */
