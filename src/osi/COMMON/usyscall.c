/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_syscall.h>
#include <osi/COMMON/usyscall.h>
#include <osi/COMMON/usyscall_impl.h>
#include <afs/afs_args.h>

struct osi_syscall_state osi_syscall_state = { 
    0,  /* sys_online */
    0,  /* ioctl_online */
    0,  /* ioctl_persistent */
    0,  /* trace_online */
    -1  /* ioctl_fd */
};

#if !defined(OSI_SYSCALL_MUX_CALL_OVERRIDE)
osi_result
osi_syscall_mux_call(long call, long p1, long p2, long p3, long p4, int * rval)
{
    osi_result res = OSI_OK;
    int code;

    if (osi_syscall_state.sys_online) {
	*rval = code = syscall(AFS_SYSCALL, call, p1, p2, p3, p4);
	if (osi_compiler_expect_false(code < 0)) {
	    goto error;
	}
    } else {
	res = OSI_ERROR_NOSYS;
    }

 done:
    return res;

 error:
    if (errno == ENOSYS) {
	res = OSI_ERROR_NOSYS;
    } else {
	res = OSI_FAIL;
    }
    goto done;
}

osi_static osi_result
osi_syscall_user_os_PkgInit(void)
{
    return OSI_OK;
}

osi_static osi_result
osi_syscall_user_os_PkgShutdown(void)
{
    return OSI_OK;
}
#endif /* !OSI_SYSCALL_MUX_CALL_OVERRIDE */

osi_result
osi_syscall_user_PkgInit(void)
{
    osi_result res;

    (void) osi_syscall_user_os_PkgInit();

    res = osi_syscall_probe();
}

osi_result
osi_syscall_user_PkgShutdown(void)
{
    return osi_syscall_user_os_PkgShutdown();
}
