/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.	For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_PARAM_H
#define AFS_PARAM_H

#ifndef UKERNEL

/* This section for kernel libafs compiles only */
#define AFS_I386_LINUX20_ENV	1
#define AFS_I386_LINUX22_ENV	1
#define AFS_I386_LINUX24_ENV	1
#define AFS_I386_UMLINUX20_ENV	1
#define AFS_I386_UMLINUX22_ENV	1
#define AFS_I386_UMLINUX24_ENV	1

#ifdef AFS_LARGEFILE_ENV
#define _FILE_OFFSET_BITS	64
#endif

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */
#define UKERNEL			1	/* user space kernel */

#endif /* !defined(UKERNEL) */

#define SYS_NAME		"i386_umlinux24"
#define SYS_NAME_ID		SYS_NAME_ID_i386_umlinux24
#define AFSLITTLE_ENDIAN	1
#define AFS_SYSCALL		137

#endif /* AFS_PARAM_H */
