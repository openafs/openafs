/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_UTIME_APPROX_H
#define _OSI_SOLARIS_UTIME_APPROX_H 1

/*
 * osi abstraction
 * approximate time interface
 * solaris userspace implementation
 */

#include <sys/time.h>
#if defined(OSI_ENV_LWP)
#include <lwp/lwp.h>
#endif


#if defined(OSI_ENV_PTHREAD) && (defined(__sparcv8plus) || defined(__sparcv9) || defined(__amd64))
/*
 * WARNING
 * this implementation makes certain ISA assumptions.  if you attempt
 * to build on the wrong platform, 64-bit load/store atomicity assumptions
 * go away, and this algorithm becomes prone to races.
 *
 * if you really must build libosi on ia32 or prehistoric sparc,
 * then you'll be using the inline implementation of osi_time_approx_get()
 * from src/osi/SOLARIS/utime_approx_inline.h
 */
#define OSI_IMPLEMENTS_TIME_APPROX 1
#define OSI_IMPLEMENTS_NATIVE_TIME_APPROX 1
#endif


#if defined(OSI_IMPLEMENTS_NATIVE_TIME_APPROX)
osi_extern osi_result osi_time_approx_get(osi_time_t *,
					  osi_time_suseconds_t samp_freq);

OSI_INIT_FUNC_PROTOTYPE(osi_time_approx_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_time_approx_PkgShutdown);
#endif /* OSI_IMPLEMENTS_NATIVE_TIME_APPROX */

#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/SOLARIS/utime_approx_inline.h>
#endif

#endif /* _OSI_SOLARIS_UTIME_APPROX_H */
