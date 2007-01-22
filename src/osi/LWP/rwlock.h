/*
 * Copyright 2005-2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LWP_RWLOCK_H
#define	_OSI_LWP_RWLOCK_H

#define OSI_IMPLEMENTS_RWLOCK 1
#define OSI_IMPLEMENTS_RWLOCK_NBRDLOCK 1
#define OSI_IMPLEMENTS_RWLOCK_NBWRLOCK 1

#include <lwp/lwp.h>
#include <lwp/lock.h>

typedef struct osi_rwlock {
    struct Lock lock;
    int level;
    osi_rwlock_options_t opts;
} osi_rwlock_t;


osi_extern void osi_rwlock_Init(osi_rwlock_t *, osi_rwlock_options_t *);
osi_extern void osi_rwlock_Destroy(osi_rwlock_t *);

#define osi_rwlock_RdLock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ObtainReadLock(&(x)->lock); \
            (x)->level++; \
        } \
    osi_Macro_End

#define osi_rwlock_WrLock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ObtainWriteLock(&(x)->lock); \
            (x)->level = -1; \
        } \
    osi_Macro_End

#define osi_rwlock_Unlock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            if ((x)->level == -1) { \
                (x)->level = 0; \
                ReleaseWriteLock(&(x)->lock); \
	    } else { \
                (x)->level--; \
	        ReleaseReadLock(&(x)->lock); \
            } \
        } \
    osi_Macro_End

osi_extern int osi_rwlock_NBRdLock(osi_rwlock_t *);
osi_extern int osi_rwlock_NBWrLock(osi_rwlock_t *);


#endif /* _OSI_LWP_RWLOCK_H */
