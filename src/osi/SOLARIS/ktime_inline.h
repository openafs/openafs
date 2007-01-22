/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KTIME_INLINE_H
#define	_OSI_SOLARIS_KTIME_INLINE_H



/* epoch time implementation */
osi_inline_define(
osi_result
osi_time_get32(osi_time32_t * t)
{
#if defined(AFS_SUN57_ENV)
    struct timeval32 tv;
    uniqtime32(&tv);
#else
    struct timeval tv;
    uniqtime(&tv);
#endif
    *t = (osi_time32_t) tv.tv_sec;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_time_get32(osi_time32_t * t)
)

osi_inline_define(
osi_result
osi_time_get64(osi_time64_t * t)
{
    struct timeval tv;
    uniqtime(&tv);
    *t = (osi_time64_t) tv.tv_sec;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_time_get64(osi_time64_t * t)
)

osi_inline_define(
osi_result
osi_time_get(osi_time_t * t)
{
    struct timeval tv;
    uniqtime(&tv);
    *t = (osi_time_t) tv.tv_sec;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_time_get(osi_time_t * t)
)

/* timeval implementation */
osi_inline_define(
osi_result
osi_timeval_unique_get64(osi_timeval64_t * t)
{
    struct timeval tv;
    uniqtime(&tv);
    t->tv_sec = (osi_time64_t) tv.tv_sec;
    t->tv_usec = (osi_time_suseconds64_t) tv.tv_usec;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_timeval_unique_get64(osi_timeval64_t * t)
)

/* timespec implementation */

osi_inline_define(
osi_result
osi_timespec_unique_get32(osi_timespec32_t * t)
{
    struct timespec ts;
    gethrestime(&ts);
    t->tv_sec = (osi_time32_t) ts.tv_sec;
    t->tv_nsec = (osi_int32) ts.tv_nsec;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_timespec_unique_get32(osi_timespec32_t * t)
)

osi_inline_define(
osi_result
osi_timespec_unique_get64(osi_timespec64_t * t)
{
    struct timespec ts;
    gethrestime(&ts);
    t->tv_sec = (osi_time64_t) ts.tv_sec;
    t->tv_nsec = (osi_int64) ts.tv_nsec;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_timespec_unique_get64(osi_timespec64_t * t)
)

/* ticks implementation */
#if defined(OSI_SUN57_ENV)
osi_inline_define(
osi_result
osi_time_ticks_get(osi_time_ticks_t * t)
{
    *t = gethrtime();
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_time_ticks_get(osi_time_ticks_t * t)
)

osi_inline_define(
osi_result
osi_time_ticks_get32(osi_time_ticks32_t * t)
{
    *t = (osi_time_ticks32_t) gethrtime();
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_time_ticks_get32(osi_time_ticks32_t * t)
)

osi_inline_define(
osi_result
osi_time_ticks_get64(osi_time_ticks64_t * t)
{
    *t = (osi_time_ticks64_t) gethrtime();
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_time_ticks_get64(osi_time_ticks64_t * t)
)

osi_inline_define(
osi_result
osi_time_ticks_unscaled_get(osi_time_ticks_t * t)
{
    *t = gethrtime_unscaled();
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_time_ticks_unscaled_get(osi_time_ticks_t * t)
)

osi_inline_define(
osi_result
osi_time_ticks_unscaled_get32(osi_time_ticks32_t * t)
{
    *t = (osi_time_ticks32_t) gethrtime_unscaled();
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_time_ticks_unscaled_get32(osi_time_ticks32_t * t)
)

osi_inline_define(
osi_result
osi_time_ticks_unscaled_get64(osi_time_ticks64_t * t)
{
    *t = (osi_time_ticks64_t) gethrtime_unscaled();
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_time_ticks_unscaled_get64(osi_time_ticks64_t * t)
)
#endif /* OSI_SUN57_ENV */

#endif /* _OSI_SOLARIS_KTIME_INLINE_H */
