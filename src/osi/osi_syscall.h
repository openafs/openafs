/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_SYSCALL_H
#define _OSI_OSI_SYSCALL_H 1

/*
 * platform-independent osi_syscall API
 *
 * the following interfaces require:
 * defined(OSI_KERNELSPACE_ENV) || defined(UKERNEL)
 *
 *  void osi_syscall_suser_check(int * retcode);
 *    -- checks for superuser privs and sets return code
 *
 *  int osi_syscall_do_32u_64k_trans(void);
 *    -- macro that checks whether syscall arg type translation 
 *       needs to happen
 *
 * the following interfaces require:
 * defined(OSI_USERSPACE_ENV)
 *
 *  osi_result osi_syscall_mux_call(long call, long p1, long p2, long p3, long p4, int * rval);
 *    -- call into the afs osi syscall mux
 */

#include <osi/COMMON/syscall.h>


#if defined(OSI_KERNELSPACE_ENV)

#include <osi/COMMON/ksyscall.h>

#define osi_syscall_user_PkgInit()  (OSI_OK)
#define osi_syscall_user_PkgShutdown()  (OSI_OK)

#elif defined(UKERNEL)

#include <osi/COMMON/ksyscall.h>
#include <osi/COMMON/usyscall.h>

#else /* !KERNEL */

#include <osi/COMMON/usyscall.h>

#define osi_syscall_kernel_PkgInit()  (OSI_OK)
#define osi_syscall_kernel_PkgShutdown()  (OSI_OK)

#endif /* !KERNEL */


#endif /* _OSI_OSI_SYSCALL_H */
