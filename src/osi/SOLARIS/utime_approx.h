/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_UTIME_APPROX_H
#define	_OSI_SOLARIS_UTIME_APPROX_H

#include <sys/time.h>
#if !defined(OSI_PTHREAD_ENV)
#include <lwp/lwp.h>
#endif

#define OSI_IMPLEMENTS_TIME_APPROX 1

osi_extern osi_result osi_time_approx_get(osi_time_t *,
					  osi_time_suseconds_t samp_freq);

#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/SOLARIS/utime_approx_inline.h>
#endif

#endif /* _OSI_SOLARIS_UTIME_APPROX_H */
