/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_TIME_H
#define _OSI_COMMON_TIME_H 1


/*
 * backwards compat logic
 */

#if defined(OSI_IMPLEMENTS_TIME_TIMEVAL_UNIQUE_GET)
#if !defined(OSI_IMPLEMENTS_TIME_TIMEVAL_NON_UNIQUE_GET)
#define OSI_IMPLEMENTS_TIME_TIMEVAL_NON_UNIQUE_GET 1
#define OSI_IMPLEMENTS_LEGACY_TIME_TIMEVAL_NON_UNIQUE_GET 1
#define osi_timeval_get(t) osi_timeval_unique_get(t)
#define osi_timeval_get32(t) osi_timeval_unique_get32(t)
#define osi_timeval_get64(t) osi_timeval_unique_get64(t)
#endif /* !OSI_IMPLEMENTS_TIME_TIMEVAL_NON_UNIQUE_GET */
#endif /* OSI_IMPLEMENTS_TIME_TIMEVAL_UNIQUE_GET */

#if defined(OSI_IMPLEMENTS_TIME_TIMESPEC_UNIQUE_GET)
#if !defined(OSI_IMPLEMENTS_TIME_TIMESPEC_NON_UNIQUE_GET)
#define OSI_IMPLEMENTS_TIME_TIMESPEC_NON_UNIQUE_GET 1
#define OSI_IMPLEMENTS_LEGACY_TIME_TIMESPEC_NON_UNIQUE_GET 1
#define osi_timespec_get(t) osi_timespec_unique_get(t)
#define osi_timespec_get32(t) osi_timespec_unique_get32(t)
#define osi_timespec_get64(t) osi_timespec_unique_get64(t)
#endif /* !OSI_IMPLEMENTS_TIME_TIMESPEC_NON_UNIQUE_GET */
#endif /* OSI_IMPLEMENTS_TIME_TIMESPEC_UNIQUE_GET */

#endif /* _OSI_COMMON_TIME_H */
