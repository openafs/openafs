/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_CURSOR_H
#define _OSI_TRACE_KERNEL_CURSOR_H 1

/*
 * osi_trace cursor support
 */

osi_extern int osi_trace_cursor_sys_read(long p1, long p2, long p3, long * retVal);
osi_extern int osi_trace_cursor_sys_readv(long p1, long p2, long p3, long * retVal);
#if (OSI_DATAMODEL != OSI_ILP32_ENV)
osi_extern int osi_trace_cursor_sys32_readv(long p1, long p2, long p3, long * retVal);
#endif /* OSI_DATAMODEL != OSI_ILP32_ENV */

#endif /* _OSI_TRACE_KERNEL_CURSOR_H */
