/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_syscall.h>

OSI_INIT_FUNC_DECL(osi_syscall_PkgInit)
{
    osi_result res;

    /* because of UKERNEL, 
     * we can have a need to init both in the same process */

    res = osi_syscall_kernel_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_syscall_user_PkgInit();

 error:
    return res;
}

OSI_FINI_FUNC_DECL(osi_syscall_PkgShutdown)
{
    osi_result res;

    res = osi_syscall_kernel_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_syscall_user_PkgShutdown();

 error:
    return res;
}
