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
#define AFS_SPARC_LINUX20_ENV	1
#define AFS_SPARC_LINUX22_ENV	1
#define AFS_SPARC_LINUX24_ENV	1

#if defined(__KERNEL__) && !defined(KDUMP_KERNEL)
#if defined(AFS_SMP) && defined(CONFIG_MODVERSIONS)
/* hack, I don't know what else with theese symbols */
#define _do_spin_lock _do_spin_lock_R__ver__do_spin_lock
#define _do_spin_unlock _do_spin_unlock_R__ver__do_spin_unlock
#define kernel_flag kernel_flag_R__ver_kernel_flag
#endif
#endif /* __KERNEL__  && !DUMP_KERNEL */

/*
 * on sparclinux is O_LARGEFILE defined but there is not off64_t,
 * so small hack to get usd_file.c work
 */
#ifndef KERNEL
#define __USE_FILE_OFFSET64	1
#define __USE_LARGEFILE64	1
#if !defined off64_t
#define off64_t __off64_t
#endif
#endif

#else /* !defined(UKERNEL) */

/* This section for user space compiles only */
#define UKERNEL			1	/* user space kernel */

#endif /* !defined(UKERNEL) */

/* Machine / Operating system information */
#define SYS_NAME		"sparc_linux24"
#define SYS_NAME_ID		SYS_NAME_ID_sparc_linux24
#define AFS_SYSCALL		227
#define AFSBIG_ENDIAN		1

#endif /* AFS_PARAM_H */
