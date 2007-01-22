/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_UTIME_H
#define	_OSI_LEGACY_UTIME_H

#include <sys/time.h>

#define OSI_IMPLEMENTS_TIME 1
#define OSI_IMPLEMENTS_TIME_TIMEVAL 1
#define OSI_IMPLEMENTS_TIME_TIMEVAL_UNIQUE_GET 1
#define OSI_IMPLEMENTS_TIME_TIMESPEC 1

typedef time_t osi_time_t;
typedef suseconds_t osi_time_suseconds_t;
typedef struct timeval osi_timeval_t;
typedef struct timespec osi_timespec_t;

typedef osi_int32 osi_time32_t;
typedef osi_int32 osi_time_suseconds32_t;
typedef osi_int64 osi_time64_t;
typedef osi_int64 osi_time_suseconds64_t;
typedef struct {
    osi_time32_t tv_sec;
    osi_time_suseconds32_t tv_usec;
} osi_timeval32_t;
typedef struct {
    osi_time32_t tv_sec;
    osi_int32 tv_nsec;
} osi_timespec32_t;
typedef struct {
    osi_time64_t tv_sec;
    osi_time_suseconds64_t tv_usec;
} osi_timeval64_t;
typedef struct {
    osi_time64_t tv_sec;
    osi_int64 tv_nsec;
} osi_timespec64_t;


#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/LEGACY/utime_inline.h>
#endif

#endif /* _OSI_LEGACY_UTIME_H */
