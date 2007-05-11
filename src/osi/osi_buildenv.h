/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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
 *  OSI_ENV_UNIX
 *    -- code is being compiled on a unix-like platform
 *
 *  OSI_ENV_KERNELSPACE
 *    -- code is being compiled for use in the kernel
 *
 *  OSI_ENV_USERSPACE
 *    -- code is being compiled for use in a userspace context
 *
 *  OSI_ENV_SMALLSTACK
 *    -- code is being compiled for use in an environment
 *       with small stack sizes (e.g. many unix kernels)
 *
 *  OSI_ENV_LITTLE_ENDIAN
 *    -- code is being compiled on a little-endian architecture
 *
 *  OSI_ENV_BIG_ENDIAN
 *    -- code is being compiled on a big-endian architecture
 *
 *  OSI_ENV_PTHREAD
 *    -- code is being compiled with POSIX threads
 *
 *  OSI_ENV_NT_THREAD
 *    -- code is being compiled with Windows NT threads
 *
 *  OSI_ENV_PREEMPTIVE
 *    -- code is being compiled in a preemptive threading environment
 *
 *  OSI_ENV_LWP
 *    -- code is being compiled with legacy non-preemptive threading environment
 *
 *  OSI_ENV_COMERR_BUILD
 *    -- build with static error code table (during comerr bootstrap)
 *
 *  OSI_ENV_INLINE_BUILD
 *    -- build non-static implementation for C99-style inlines
 *
 *  OSI_ENV_INLINE_INCLUDE
 *    -- whether to include inline definition in this compilation unit
 *       (defaults to true except for OSI_ENV_INLINE_BUILD compilation units)
 *
 *  OSI_ENV_DEBUG
 *    -- one (or more) libosi debugging features are enabled
 */

#if defined(OSI_ENV_KERNELSPACE) && !defined(KERNEL)
#define KERNEL 1
#endif

#if !defined(OSI_ENV_KERNELSPACE) && !defined(OSI_ENV_USERSPACE)
#if defined(KERNEL) && !defined(UKERNEL)
#define OSI_ENV_KERNELSPACE 1
#else
#define OSI_ENV_USERSPACE 1
#endif
#endif /* !OSI_ENV_KERNELSPACE && !OSI_ENV_USERSPACE */

/* for now, we'll turn this on for all kernels */
#if defined(OSI_ENV_KERNELSPACE)
#define OSI_ENV_SMALLSTACK 1
#endif


#if defined(WORDS_BIGENDIAN)
#define OSI_ENV_BIG_ENDIAN 1
#else
#define OSI_ENV_LITTLE_ENDIAN 1
#endif

#if !defined(OSI_ENV_PTHREAD) && defined(AFS_PTHREAD_ENV)
#define OSI_ENV_PTHREAD 1
#endif
#if defined(OSI_ENV_PTHREAD) && !defined(AFS_PTHREAD_ENV)
#define AFS_PTHREAD_ENV 1
#endif

/*
 * XXX we eventually need to make this explicit on the
 * build command line, as userspace && !pthread && !ukernel => lwp 
 * is a false implication
 */
#if !defined(OSI_ENV_LWP) && defined(OSI_ENV_USERSPACE) && !defined(OSI_ENV_PTHREAD) && !defined(UKERNEL)
#define OSI_ENV_LWP 1
#endif

#if defined(OSI_ENV_KERNELSPACE) || defined(OSI_ENV_PTHREAD) || defined(OSI_ENV_NT_THREAD)
#define OSI_ENV_PREEMPTIVE 1
#endif

#if defined(OSI_BUILD_INLINES)
#define OSI_ENV_INLINE_BUILD 1
#endif

#if defined(OSI_ENV_PREEMPTIVE)
#define osi_volatile_mt osi_volatile
#else
#define osi_volatile_mt
#endif

#endif /* _OSI_OSI_BUILDENV_H */
