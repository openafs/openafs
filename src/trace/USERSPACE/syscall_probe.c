/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi Trace syscall probe utility
 *
 * this subsystem attempts to determine whether
 * an osi_Trace capable kernel module is loaded
 */

#include <osi/osi_impl.h>
#include <osi/osi_syscall.h>
#include <osi/osi_signal.h>
#include <osi/COMMON/usyscall.h>
#include <osi/COMMON/usyscall_impl.h>
#include <trace/syscall.h>

#include <afs/afs_args.h>

osi_static void
osi_Trace_syscall_probe_sigsys_handler(osi_signal_t sig);

osi_result
osi_Trace_syscall_probe(void)
{
    int rval;
    osi_result code;
    osi_signal_handler_t * old;

    /* install our custom sigsys handler */
    old = osi_signal_handler(SIGSYS, 
			     &osi_Trace_syscall_probe_sigsys_handler);

    code = osi_syscall_mux_call(AFSCALL_OSI_TRACE,
				OSI_TRACE_SYSCALL_OP_NULL,
				0, 0, 0, &rval);
    if (OSI_RESULT_OK(code)) {
	osi_syscall_state.trace_online = 1;
	goto sys_online;
    } else {
	osi_syscall_state.trace_online = 0;
	if (code == OSI_ERROR_NOSYS) {
	    (osi_Msg "osi_Trace_syscall_probe(): osi_Trace_syscall mux does not exist; trace rendered inoperable.\n");
	} else if (errno == EPERM) {
	    (osi_Msg "osi_Trace_syscall_probe(): process does not have appropriate credentials; trace rendered inoperable.\n");
	} else {
	    (osi_Msg "osi_Trace_syscall_probe(): unknown error %d, errno=%d\n", code, errno);
	}
    }

 sys_online:
    /* restore old sigsys handler */
    (void)osi_signal_handler(SIGSYS, old);
    return OSI_OK;
}


/*
 * drop sys_online flag on SIGSYS
 */
osi_static void
osi_Trace_syscall_probe_sigsys_handler(osi_signal_t sig)
{
    osi_syscall_state.trace_online = 0;
}
