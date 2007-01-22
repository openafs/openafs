/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KTIME_H
#define	_OSI_SOLARIS_KTIME_H

#include <sys/time.h>

#define OSI_IMPLEMENTS_TIME 1

typedef time_t osi_time_t;
typedef suseconds_t osi_time_suseconds_t;
typedef struct timeval osi_timeval_t;
typedef struct timespec osi_timespec_t;

#if defined(AFS_SUN57_ENV)
typedef time32_t osi_time32_t;
typedef osi_int32 osi_time_suseconds32_t;
typedef struct timeval32 osi_timeval32_t;
typedef struct timespec32 osi_timespec32_t;
#else
typedef time_t osi_time3_t;
typedef suseconds_t osi_time_suseconds32_t;
typedef struct timeval osi_timeval32_t;
typedef struct timespec osi_timespec32_t;
#endif
typedef osi_int64 osi_time64_t;
typedef osi_int64 osi_time_suseconds64_t;
typedef struct {
    osi_time64_t tv_sec;
    osi_time_suseconds64_t tv_usec;
} osi_timeval64_t;
typedef struct {
    osi_time64_t tv_sec;
    osi_int64 tv_nsec;
} osi_timespec64_t;


/* timeval implementation */
#define OSI_IMPLEMENTS_TIME_TIMEVAL 1
#define osi_timeval_unique_get(t) (uniqtime(t),OSI_OK)
#if defined(AFS_SUN57_ENV)
#define osi_timeval_unique_get32(t) (uniqtime32(t),OSI_OK)
#else
#define osi_timeval_unique_get32(t) (uniqtime(t),OSI_OK)
#endif

/* timespec implementation */
#define OSI_IMPLEMENTS_TIME_TIMESPEC 1

#define osi_timespec_unique_get(t) (gethrestime(t), OSI_OK)

/* unscaled ticks implementation */
#if defined(AFS_SUN57_ENV)
#define OSI_IMPLEMENTS_TIME_TICKS 1
#define OSI_IMPLEMENTS_TIME_TICKS_SCALED_GET 1
#define OSI_IMPLEMENTS_TIME_TICKS_UNSCALED_GET 1
typedef osi_uint32 osi_time_ticks32_t;
typedef osi_uint64 osi_time_ticks64_t;
typedef hrtime_t osi_time_ticks_t;
#endif


#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/SOLARIS/ktime_inline.h>
#endif

#endif /* _OSI_SOLARIS_KTIME_H */
