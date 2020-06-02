/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *
 * DFBSD OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

static_inline void
osi_GetTime(osi_timeval32_t *atv)
{
    struct timeval now;
    microtime(&now);
    atv->tv_sec = now.tv_sec;
    atv->tv_usec = now.tv_usec;
}

#endif /* _OSI_MACHDEP_H_ */
