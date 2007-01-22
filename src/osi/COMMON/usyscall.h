/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_USYSCALL_H
#define _OSI_COMMON_USYSCALL_H 1

#if defined(OSI_LINUX20_ENV)

#include <osi/LINUX/usyscall.h>

#elif defined(OSI_DARWIN80_ENV)

#include <osi/DARWIN/usyscall.h>

#endif

osi_extern osi_result osi_syscall_mux_call(long call, 
					   long p1, long p2, 
					   long p3, long p4, 
					   int * rval);

osi_extern osi_result osi_syscall_user_PkgInit(void);
osi_extern osi_result osi_syscall_user_PkgShutdown(void);

#endif /* _OSI_COMMON_USYSCALL_H */
