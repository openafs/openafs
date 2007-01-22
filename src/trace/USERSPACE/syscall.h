/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_USERSPACE_SYSCALL_H
#define _OSI_TRACE_USERSPACE_SYSCALL_H 1



/*
 * osi_trace syscall support
 */

osi_extern osi_result osi_Trace_syscall(long opcode, long p1, long p2, long p3, int * retval);

#endif /* _OSI_TRACE_USERSPACE_SYSCALL_H */
