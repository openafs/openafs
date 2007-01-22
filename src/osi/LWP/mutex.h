/*
 * Copyright 2005-2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LWP_MUTEX_H
#define	_OSI_LWP_MUTEX_H

#define OSI_IMPLEMENTS_MUTEX 1
#define OSI_IMPLEMENTS_MUTEX_NBLOCK 1

#include <lwp.h>
#include <lock.h>

typedef struct osi_mutex {
    struct Lock lock;
    osi_mutex_options_t opts;
} osi_mutex_t;

extern void osi_mutex_Init(osi_mutex_t *, osi_mutex_options_t *);
extern void osi_mutex_Destroy(osi_mutex_t *);

#define osi_mutex_Lock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ObtainWriteLock(&(x)->lock); \
        } \
    osi_Macro_End

#define osi_mutex_Unlock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ReleaseWriteLock(&(x)->lock); \
        } \
    osi_Macro_End

extern int osi_mutex_NBLock(osi_mutex_t *);

#endif /* _OSI_LWP_MUTEX_H */
