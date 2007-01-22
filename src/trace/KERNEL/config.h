/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_CONFIG_H
#define _OSI_TRACE_KERNEL_CONFIG_H 1


/*
 * tracepoint kernel interfaces
 */

#include <trace/syscall.h>

osi_extern osi_result osi_trace_config_get(osi_trace_syscall_config_t * cfg);

/* syscall interface */
osi_extern int osi_trace_config_sys_get(long p1);
#if (OSI_DATAMODEL != OSI_ILP32_ENV)
osi_extern int osi_trace_config_sys32_get(long p1);
#endif /* OSI_DATAMODEL != OSI_ILP32_ENV */


#endif /* _OSI_TRACE_KERNEL_CONFIG_H */
