/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_BUILDENV_H
#define _OSI_OSI_BUILDENV_H 1


/*
 * osi abstraction
 * build environment related defines
 *
 * defines:
 *
 *  OSI_KERNELSPACE_ENV
 *    -- code is being compiled for use in the kernel
 *
 *  OSI_USERSPACE_ENV
 *    -- code is being compiled for use in a userspace context
 *
 *  OSI_SMALLSTACK_ENV
 *    -- code is being compiled for use in an environment
 *       with small stack sizes (e.g. many unix kernels)
 *
 *  OSI_LITTLE_ENDIAN_ENV
 *    -- code is being compiled on a little-endian architecture
 *
 *  OSI_BIG_ENDIAN_ENV
 *    -- code is being compiled on a big-endian architecture
 *
 *  OSI_PTHREAD_ENV
 *    -- code is being compiled with POSIX threads
 *
 *  OSI_PREEMPTIVE_ENV
 *    -- code is being compiled in a preemptive threading environment
 *
 *  OSI_LWP_ENV
 *    -- code is being compiled with legacy non-preemptive threading environment
 */

#if defined(KERNEL) && !defined(UKERNEL)
#define OSI_KERNELSPACE_ENV 1
#else
#define OSI_USERSPACE_ENV 1
#endif


/* for now, we'll turn this on for all kernels */
#if defined(OSI_KERNELSPACE_ENV)
#define OSI_SMALLSTACK_ENV 1
#endif


#if defined(AFSLITTLE_ENDIAN)
#define OSI_LITTLE_ENDIAN_ENV 1
#elif defined(AFSBIG_ENDIAN)
#define OSI_BIG_ENDIAN_ENV 1
#else
#error "platform is of unknown endianness"
#endif

#if !defined(OSI_PTHREAD_ENV) && defined(AFS_PTHREAD_ENV)
#define OSI_PTHREAD_ENV 1
#endif
#if defined(OSI_PTHREAD_ENV) && !defined(AFS_PTHREAD_ENV)
#define AFS_PTHREAD_ENV 1
#endif

/*
 * XXX we eventually need to make this explicit on the
 * build command line, as userspace && !pthread && !ukernel => lwp 
 * is a false implication
 */
#if !defined(OSI_LWP_ENV) && defined(OSI_USERSPACE_ENV) && !defined(OSI_PTHREAD_ENV) && !defined(UKERNEL)
#define OSI_LWP_ENV 1
#endif

#if defined(OSI_KERNELSPACE_ENV) || defined(OSI_PTHREAD_ENV) || defined(OSI_NT_THREAD_ENV)
#define OSI_PREEMPTIVE_ENV 1
#endif

#if defined(OSI_BUILD_INLINES)
#define OSI_ENV_INLINE_BUILD 1
#endif

#if defined(OSI_PREEMPTIVE_ENV)
#define osi_volatile_mt osi_volatile
#else
#define osi_volatile_mt
#endif

#endif /* _OSI_OSI_BUILDENV_H */
