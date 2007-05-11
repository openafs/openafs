/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_syscall.h>
#include <osi/COMMON/usyscall.h>
#include <osi/COMMON/usyscall_impl.h>
#include <trace/USERSPACE/syscall.h>
#include <trace/USERSPACE/syscall_probe.h>

#include <afs/afs_args.h>

/*
 * osi tracing framework
 * trace syscall
 */

osi_result
osi_Trace_syscall(long opcode, long p1, long p2, long p3, int * retval)
{
    osi_result code;

    if (osi_syscall_state.trace_online) {
	code = osi_syscall_mux_call(AFSCALL_OSI_TRACE, opcode, p1, p2, p3, retval);
    } else {
	code = OSI_ERROR_NOSYS;
    }

    return code;
}

osi_result
osi_trace_syscall_PkgInit(void)
{
    return osi_Trace_syscall_probe();
}

osi_result
osi_trace_syscall_PkgShutdown(void)
{
    return OSI_OK;
}
