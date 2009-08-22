/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_PARAM_H
#define AFS_PARAM_H

#ifndef UKERNEL

/* This section for kernel libafs compiles only */
#define AFS_S390_LINUX20_ENV	1
#define AFS_S390_LINUX22_ENV	1
#define AFS_S390_LINUX24_ENV	1
#define AFS_S390X_LINUX20_ENV	1
#define AFS_S390X_LINUX22_ENV	1
#define AFS_S390X_LINUX24_ENV	1

#define AFS_64BITPOINTER_ENV	1
#define AFS_64BIT_KERNEL	1

#if defined(__KERNEL__) && !defined(KDUMP_KERNEL)
#ifdef AFS_SMP
#ifndef CONFIG_S390_LOCAL_APIC
#define CONFIG_S390_LOCAL_APIC 1
#endif
#endif
#endif /* __KERNEL__  && !DUMP_KERNEL */

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */
#define UKERNEL			1	/* user space kernel */
#define AFS_S390X_LINUX20_ENV	1
#define AFS_S390X_LINUX22_ENV	1
#define AFS_S390X_LINUX24_ENV	1

#define AFS_64BITPOINTER_ENV	1

#endif /* !defined(UKERNEL) */

/* Machine / Operating system information */
#define SYS_NAME		"s390x_linux24"
#define SYS_NAME_ID		SYS_NAME_ID_s390x_linux24
#define AFS_SYSCALL		137
#define AFSBIG_ENDIAN		1

#endif /* AFS_PARAM_H */
