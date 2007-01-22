/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_syscall.h>
#include <trace/syscall.h>

/*
 * osi tracing framework
 * UKERNEL support
 * trace syscall
 */

/*
 * main osi_Trace syscall handler
 */
int
osi_Trace_syscall_handler(long opcode, 
			  long p1, long p2, long p3, long p4, 
			  long * retVal)
{
    return ENOSYS;
}


osi_result
osi_trace_syscall_PkgInit(void)
{
    return OSI_OK;
}

osi_result
osi_trace_syscall_PkgShutdown(void)
{
    return OSI_OK;
}

