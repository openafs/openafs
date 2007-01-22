/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_QUERY_H
#define _OSI_TRACE_KERNEL_QUERY_H 1


/*
 * tracepoint kernel interfaces
 */

#include <trace/syscall.h>

osi_extern osi_result osi_trace_query_get(osi_trace_query_id_t, void *, size_t);

#endif /* _OSI_TRACE_KERNEL_QUERY_H */
