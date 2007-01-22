/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi syscall mux probe utility
 *
 * this subsystem attempts to determine whether
 * libafs or libosi is loaded into the kernel
 * by probing for a few well-known syscalls
 */

#include <osi/osi_impl.h>
#include <osi/osi_syscall.h>
#include <osi/osi_signal.h>
#include <osi/COMMON/usyscall.h>
#include <osi/COMMON/usyscall_impl.h>
#include <afs/afs_args.h>
#include <trace/syscall.h>
#include <netinet/in.h>
#include <afs/vice.h>
#include <afs/venus.h>

osi_static void
osi_syscall_probe_sigsys_handler(osi_signal_t sig);

osi_result
osi_syscall_probe(void)
{
    int code;
    struct ViceIoctl ioc;
    osi_uint32 blob[16];
    osi_signal_handler_t * old;

    /* install our custom sigsys handler */
    old = osi_signal_handler(SIGSYS, 
			     &osi_syscall_probe_sigsys_handler);

    /* try out a couple no-op osi and afs syscalls to see if
     * the syscall mux is up and running */

    osi_syscall_state.sys_online = 1;
    code = syscall(AFS_SYSCALL,
		   AFSCALL_OSI_TRACE,
		   OSI_TRACE_SYSCALL_OP_NULL);
    if (!code) {
	goto sys_online;
    }

    /* reinstate in case it fired and was reset to SIG_DFL */
    (void)osi_signal_handler(SIGSYS,
			     &osi_syscall_probe_sigsys_handler);

    osi_syscall_state.sys_online = 1;
    ioc.in = osi_NULL;
    ioc.in_size = 0;
    ioc.out = (char *) blob;
    ioc.out_size = sizeof(blob);

    code = syscall(AFS_SYSCALL, 
		   AFSCALL_PIOCTL,
		   osi_NULL,
		   VIOCGETCACHEPARMS,
		   &blob,
		   1);

    if (!code) {
	goto sys_online;
    } else if (errno == ENOSYS) {
	osi_syscall_state.sys_online = 0;
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
osi_syscall_probe_sigsys_handler(osi_signal_t sig)
{
    osi_syscall_state.sys_online = 0;
}
