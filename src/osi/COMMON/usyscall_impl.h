/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_USYSCALL_IMPL_H
#define _OSI_COMMON_USYSCALL_IMPL_H 1

struct osi_syscall_state {
    osi_uint8 sys_online;        /* syscall(2) driven backend is online */
    osi_uint8 ioctl_online;      /* ioctl(2) fallback backend is online */
    osi_uint8 ioctl_persistent;  /* ioctl fd should be held open */
    osi_uint8 trace_online;      /* whether osi_Trace_syscall mux is online */
    osi_fd_t ioctl_fd;           /* persistent fd for ioctl backend */
};

osi_extern struct osi_syscall_state osi_syscall_state;

osi_extern osi_result osi_syscall_probe(void);

#endif /* _OSI_COMMON_USYSCALL_IMPL_H */
