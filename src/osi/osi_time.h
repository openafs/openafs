/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_TIME_H
#define _OSI_OSI_TIME_H 1

/*
 * platform-independent osi_time API
 *
 * types:
 *  osi_time_t;
 *    -- native-sized scaled time in seconds
 *  osi_time32_t;
 *    -- 32-bit scaled time in seconds
 *  osi_time64_t;
 *    -- 64-bit scaled time in seconds
 *  osi_time_suseconds_t;
 *    -- native-sized scaled time in microseconds
 *  osi_time_suseconds32_t;
 *    -- 32-bit scaled time in microseconds
 *  osi_time_suseconds64_t;
 *    -- 64-bit scaled time in microseconds
 *
 *  osi_result osi_time_get(osi_time_t *);
 *  osi_result osi_time_get32(osi_time32_t *);
 *  osi_result osi_time_get64(osi_time64_t *);
 *    -- get the exact time stamp
 *
 * approximate time API
 *
 *  osi_result osi_time_approx_get(osi_time_t *, osi_time_suseconds_t interval);
 *  osi_result osi_time_approx_get32(osi_time32_t *, osi_time_suseconds_t interval);
 *  osi_result osi_time_approx_get64(osi_time64_t *, osi_time_suseconds_t interval);
 *    -- get the current time stamp approximation,
 *       using sampling interval $interval$ (a la nyquist)
 *       (accurate to within $interval * 2$ microseconds)
 *       
 *
 * the following interfaces require:
 * defined(OSI_IMPLEMENTS_TIME_TIMEVAL)
 *
 * types:
 *  osi_timeval_t;
 *    -- native-sized timeval structure
 *  osi_timeval32_t;
 *    -- timeval structure with 32-bit members
 *  osi_timeval64_t;
 *    -- timeval structure with 64-bit members
 *
 * the following interfaces require:
 * defined(OSI_IMPLEMENTS_TIME_TIMEVAL_NON_UNIQUE_GET)
 *
 *  osi_result osi_timeval_get(osi_timeval_t *);
 *  osi_result osi_timeval_get32(osi_timeval32_t *);
 *  osi_result osi_timeval_get64(osi_timeval64_t *);
 *    -- get the current timeval (no uniqueness guarantees)
 *
 * the following interfaces require:
 * defined(OSI_IMPLEMENTS_TIME_TIMEVAL_UNIQUE_GET)
 *
 *  osi_result osi_timeval_unique_get(osi_timeval_t *);
 *  osi_result osi_timeval_unique_get32(osi_timeval32_t *);
 *  osi_result osi_timeval_unique_get64(osi_timeval64_t *);
 *    -- get the current timeval (guaranteed unique across all cpus)
 *
 * the following interfaces require:
 * defined(OSI_IMPLEMENTS_TIME_TIMESPEC)
 *
 * types:
 *  osi_timespec_t;
 *    -- native-sized timespec structure
 *  osi_timespec32_t;
 *    -- timespec structure with 32-bit members
 *  osi_timespec64_t;
 *    -- timespec structure with 64-bit members
 *
 * the following interfaces require:
 * defined(OSI_IMPLEMENTS_TIME_TIMESPEC_NON_UNIQUE_GET)
 *
 *  osi_result osi_timespec_get(osi_timespec_t *);
 *  osi_result osi_timespec_get32(osi_timespec32_t *);
 *  osi_result osi_timespec_get64(osi_timespec64_t *);
 *    -- get the current timespec (no uniqueness guarantees)
 *
 * the following interfaces require:
 * defined(OSI_IMPLEMENTS_TIME_TIMESPEC_UNIQUE_GET)
 *
 *  osi_result osi_timespec_unique_get(osi_timespec_t *);
 *  osi_result osi_timespec_unique_get32(osi_timespec32_t *);
 *  osi_result osi_timespec_unique_get64(osi_timespec64_t *);
 *    -- get the current timespec (guaranteed unique across all cpus)
 *
 * the following interfaces require:
 * defined(OSI_IMPLEMENTS_TIME_TICKS)
 *
 * types:
 *  osi_time_ticks_t;
 *    -- native-sized ticks from an arbitrary starting point
 *  osi_time_ticks32_t;
 *    -- 32-bit time in ticks from an arbitrary starting point
 *       (probably not useful, except over very small time scales)
 *  osi_time_ticks64_t;
 *    -- 64-bit time in ticks from an arbitrary starting point
 *
 * the following interfaces require:
 * defined(OSI_IMPLEMENTS_TIME_TICKS_SCALED_GET)
 *
 *  osi_result osi_time_ticks_get(osi_time_ticks_t *);
 *  osi_result osi_time_ticks_get32(osi_time_ticks32_t *);
 *  osi_result osi_time_ticks_get64(osi_time_ticks64_t *);
 *    -- get the current ticks value, scaled to be accurate across cpus
 *
 * the following interfaces require:
 * defined(OSI_IMPLEMENTS_TIME_TICKS_UNSCALED_GET)
 *
 *  osi_result osi_time_ticks_unscaled_get(osi_time_ticks_t *);
 *  osi_result osi_time_ticks_unscaled_get32(osi_time_ticks32_t *);
 *  osi_result osi_time_ticks_unscaled_get64(osi_time_ticks64_t *);
 *    -- get the current unscaled ticks value for this cpu
 *
 */


/* now include the right back-end implementation header */
#if defined(OSI_KERNELSPACE_ENV)

#if defined(AFS_SUN5_ENV)
#include <osi/SOLARIS/ktime.h>
#else
#include <osi/LEGACY/ktime.h>
#endif

#else /* !OSI_KERNELSPACE_ENV */

#if defined(AFS_SUN5_ENV)
#include <osi/SOLARIS/utime.h>
#include <osi/SOLARIS/utime_approx.h>
#else
#include <osi/LEGACY/utime.h>
#endif

#endif /* !OSI_KERNELSPACE_ENV */

#include <osi/COMMON/time_approx.h>

#endif /* _OSI_OSI_TIME_H */
