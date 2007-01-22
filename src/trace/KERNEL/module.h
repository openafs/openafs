/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_MODULE_H
#define _OSI_TRACE_KERNEL_MODULE_H 1

/*
 * modular tracepoint table support
 */

/* syscall interfaces */
osi_extern int osi_trace_module_sys_info(void *);
osi_extern int osi_trace_module_sys_lookup(long, void *);
osi_extern int osi_trace_module_sys_lookup_by_name(const char *, void *);
osi_extern int osi_trace_module_sys_lookup_by_prefix(const char *, void *);

#endif /* _OSI_TRACE_KERNEL_MODULE_H */
