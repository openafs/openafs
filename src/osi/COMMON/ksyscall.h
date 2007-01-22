/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_KSYSCALL_H
#define _OSI_COMMON_KSYSCALL_H 1

#include <osi/osi_proc.h>


#if defined(OSI_SUN5_ENV)

#include <osi/SOLARIS/ksyscall.h>

#elif defined(KERNEL_HAVE_UERROR)

#define osi_syscall_suser_check(retcode) \
    osi_Macro_Begin \
        if (!osi_proc_current_suser()) { \
	    setuerror(EACCES); \
	    *(retcode) = (EACCES); \
        } \
    osi_Macro_End

#elif defined(OSI_OSF_ENV)

/* XXX DUX is deprecated; I guess this could be tossed... */
#define osi_syscall_suser_check(retcode) \
    osi_Macro_Begin \
        if (!osi_proc_current_suser()) { \
            *(retcode) = EACCES; \
        } \
    osi_Macro_End

#else 

#define osi_syscall_suser_check(retcode) \
    osi_Macro_Begin \
        if (!osi_proc_current_suser()) { \
            *(retcode) = EPERM; \
        } \
    osi_Macro_End

#endif

/*
 * XXX this will need to change if we someday want to
 * support syscalls in a 64-bit userspace / 32-bit kernelspace
 * environment (e.g. AIX optionally supported this model)
 */
#define osi_syscall_do_32u_64k_trans() \
    (osi_proc_datamodel() != osi_datamodel())


/*
 * kernel syscall mux initialization
 *
 * for now, no-ops on all platforms.
 * eventually, we should yank the
 * syscall mux code out of src/afs.
 * raw syscall code doesn't necessarily
 * have anything to do with AFS per se
 * (as evidenced by libktrace).
 */

#define osi_syscall_kernel_PkgInit()      (OSI_OK)
#define osi_syscall_kernel_PkgShutdown()  (OSI_OK)

#endif /* _OSI_COMMON_KSYSCALL_H */
