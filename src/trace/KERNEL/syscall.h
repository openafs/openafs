/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_SYSCALL_H
#define _OSI_TRACE_KERNEL_SYSCALL_H 1



/*
 * osi_trace syscall support
 */

osi_extern int osi_Trace_syscall_handler(long opcode, 
					 long p1, 
					 long p2, 
					 long p3, 
					 long p4, 
					 long * retVal);

#endif /* _OSI_TRACE_KERNEL_SYSCALL_H */
