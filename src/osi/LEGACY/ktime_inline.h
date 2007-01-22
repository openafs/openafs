/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_KTIME_INLINE_H
#define	_OSI_LEGACY_KTIME_INLINE_H


/* epoch time implementation */
osi_inline_define(
osi_result
osi_time_get32(osi_time32_t * t)
{
    osi_timeval_t tv;
    osi_GetTime(&tv);
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
    osi_timeval_t tv;
    osi_GetTime(&tv);
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
    osi_timeval_t tv;
    osi_GetTime(&tv);
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
osi_timeval_unique_get(osi_timeval_t * t)
{
    osi_GetTime(t);
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_timeval_unique_get(osi_timeval_t * t)
)

osi_inline_define(
osi_result
osi_timeval_unique_get32(osi_timeval32_t * t)
{
    osi_timeval_t tv;
    osi_GetTime(&tv);
    t->tv_sec = (osi_time32_t) tv.tv_sec;
    t->tv_usec = (osi_time_suseconds32_t) tv.tv_usec;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_timeval_unique_get32(osi_timeval32_t * t)
)

osi_inline_define(
osi_result
osi_timeval_unique_get64(osi_timeval64_t * t)
{
    osi_timeval_t tv;
    osi_GetTime(&tv);
    t->tv_sec = (osi_time64_t) tv.tv_sec;
    t->tv_usec = (osi_time_suseconds64_t) tv.tv_usec;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_timeval_unique_get64(osi_timeval64_t * t)
)

#endif /* _OSI_LEGACY_KTIME_INLINE_H */
