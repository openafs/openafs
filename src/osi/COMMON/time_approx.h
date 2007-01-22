/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_TIME_APPROX_H
#define _OSI_COMMON_TIME_APPROX_H 1

/* default to sampling every half second */
#define OSI_TIME_APPROX_SAMP_INTERVAL_DEFAULT 500000

/*
 * when no approx time backend exists, just
 * use the regular time acquisition funcs as a backend
 */

#if !defined(OSI_IMPLEMENTS_TIME_APPROX)
#define OSI_IMPLEMENTS_TIME_APPROX 1
#define osi_time_approx_get(x, delta) osi_time_get(x)
#define osi_time_approx_get32(x, delta) osi_time_get32(x)
#define osi_time_approx_get64(x, delta) osi_time_get64(x)
#define osi_time_approx_PkgInit() (OSI_OK)
#define osi_time_approx_PkgShutdown() (OSI_OK)
#endif /* !OSI_IMPLEMENTS_TIME_APPROX */

#endif /* _OSI_COMMON_TIME_APPROX_H */
