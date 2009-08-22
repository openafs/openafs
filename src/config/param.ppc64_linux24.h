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
/* This section for kernel space compiles only */
#define AFS_PPC64_LINUX20_ENV	1
#define AFS_PPC64_LINUX22_ENV	1
#define AFS_PPC64_LINUX24_ENV	1
#define AFS_LINUX_64BIT_KERNEL	1
#define AFS_64BITPOINTER_ENV	1     /* pointers are 64 bits */

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */
#define UKERNEL			1	/* user space kernel */

#endif /* !defined(UKERNEL) */

/* Machine / Operating system information */
#define SYS_NAME	"ppc64_linux24"
#define SYS_NAME_ID	SYS_NAME_ID_ppc64_linux24
#define AFSBIG_ENDIAN	1
#define AFS_SYSCALL	137

#endif /* AFS_PARAM_H */
