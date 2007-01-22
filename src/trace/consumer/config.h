/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CONFIG_H
#define _OSI_TRACE_CONSUMER_CONFIG_H 1


/*
 * tracepoint consumer interfaces
 */

#include <trace/syscall.h>

osi_extern osi_result osi_trace_consumer_kernel_config_lookup(osi_trace_syscall_config_key_t,
							      osi_uint32 *);

osi_extern osi_result osi_trace_consumer_kernel_config_PkgInit(void);
osi_extern osi_result osi_trace_consumer_kernel_config_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSUMER_CONFIG_H */
