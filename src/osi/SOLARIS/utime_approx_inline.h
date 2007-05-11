/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_UTIME_APPROX_INLINE_H
#define _OSI_SOLARIS_UTIME_APPROX_INLINE_H 1

#if defined(OSI_IMPLEMENTS_NATIVE_TIME_APPROX)

/*
 * XXX move the following interface to src/osi/LWP
 */
#if 0
/* use the LWP approx time interface */
osi_inline_define(
osi_result
osi_time_approx_get(osi_time_t * ts_out,
		    osi_time_suseconds_t samp_interval)
{
    unsigned int ts;
    ts = FT_ApproxTime();
    *ts_out = (osi_time_t) ts;
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
osi_time_approx_get(osi_time_t * ts_out,
		    osi_time_suseconds_t samp_interval)
)
#endif /* 0 */


osi_inline_define(
osi_result 
osi_time_approx_get32(osi_time32_t * ts_out,
		      osi_time_suseconds_t samp_interval)
{
    osi_result res;
    osi_time_t ts;

    res = osi_time_approx_get(&ts, samp_interval);
    *ts_out = (osi_time32_t) ts;
}
)
osi_inline_prototype(
osi_result
osi_time_approx_get32(osi_time32_t * ts_out,
		      osi_time_suseconds_t samp_interval)
)

osi_inline_define(
osi_result 
osi_time_approx_get64(osi_time64_t * ts_out,
		      osi_time_suseconds_t samp_interval)
{
    osi_result res;
    osi_time_t ts;

    res = osi_time_approx_get(&ts, samp_interval);
    *ts_out = (osi_time64_t) ts;
}
)
osi_inline_prototype(
osi_result
osi_time_approx_get64(osi_time64_t * ts_out,
		      osi_time_suseconds_t samp_interval)
)

#endif /* OSI_IMPLEMENTS_NATIVE_TIME_APPROX */

#endif /* _OSI_SOLARIS_UTIME_APPROX_INLINE_H */
