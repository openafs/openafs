/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_UTIME_H
#define	_OSI_SOLARIS_UTIME_H

#include <time.h>

#define OSI_IMPLEMENTS_TIME 1
#define OSI_IMPLEMENTS_TIME_TIMEVAL 1
#define OSI_IMPLEMENTS_TIME_TIMEVAL_UNIQUE_GET 1
#define OSI_IMPLEMENTS_TIME_TIMESPEC 1


typedef time_t osi_time_t;
typedef suseconds_t osi_time_suseconds_t;
typedef struct timeval osi_timeval_t;
typedef struct timespec osi_timespec_t;

#if defined(OSI_SUN57_ENV)
typedef osi_int32 osi_time32_t;
typedef osi_int32 osi_time_suseconds32_t;
typedef struct {
    osi_time32_t tv_sec;
    osi_time_suseconds32_t tv_usec;
} osi_timeval32_t;
typedef struct {
    osi_time32_t tv_sec;
    osi_int32 tv_nsec;
} osi_timespec32_t;
#else
typedef time_t osi_time32_t;
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

/* 
 * non-unique timeval is currently implemented on top of unique
 * timeval on solaris (the faster non-unique methods are not
 * available outside of the kernel)
 */
#define OSI_IMPLEMENTS_TIME_TIMEVAL_NON_UNIQUE_GET 1
#define osi_timeval_get osi_timeval_unique_get
#define osi_timeval_get32 osi_timeval_unique_get32
#define osi_timeval_get64 osi_timeval_unique_get64


/* unscaled clock ticks */
#if defined(OSI_SUN57_ENV)
#define OSI_IMPLEMENTS_TIME_TICKS 1
#define OSI_IMPLEMENTS_TIME_TICKS_SCALED_GET 1
typedef osi_uint32 osi_time_ticks32_t;
typedef hrtime_t osi_time_ticks64_t;     /* hrtime_t is always 64 bits long */
typedef hrtime_t osi_time_ticks_t;

/* 
 * unscaled ticks is currently implemented on top of scaled
 * ticks on solaris (the faster unscaled methods are not
 * available outside of the kernel)
 */
#define OSI_IMPLEMENTS_TIME_TICKS_UNSCALED_GET 1
#define osi_time_ticks_unscaled_get osi_time_ticks_get
#define osi_time_ticks_unscaled_get32 osi_time_ticks_get32
#define osi_time_ticks_unscaled_get64 osi_time_ticks_get64

#endif


#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/SOLARIS/utime_inline.h>
#endif

#endif /* _OSI_SOLARIS_UTIME_H */
