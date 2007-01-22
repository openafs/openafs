/*
 * Copyright 2005-2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LWP_SHLOCK_H
#define	_OSI_LWP_SHLOCK_H


#define OSI_IMPLEMENTS_SHLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_NBRDLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_NBSHLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_NBWRLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_WTOR 1

#include <lwp/lwp.h>
#include <lwp/lock.h>

typedef struct osi_shlock {
    struct Lock lock;
    osi_shlock_options_t opts;
} osi_shlock_t;

osi_extern void osi_shlock_Init(osi_shlock_t *, osi_shlock_options_t *);
osi_extern void osi_shlock_Destroy(osi_shlock_t *);

#define osi_shlock_RdLock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ObtainReadLock(&(x)->lock); \
        } \
    osi_Macro_End

#define osi_shlock_ShLock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ObtainSharedLock(&(x)->lock); \
        } \
    osi_Macro_End

#define osi_shlock_WrLock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ObtainWriteLock(&(x)->lock); \
        } \
    osi_Macro_End

#define osi_shlock_RdUnlock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ReleaseReadLock(&(x)->lock); \
        } \
    osi_Macro_End

#define osi_shlock_ShUnlock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ReleaseSharedLock(&(x)->lock); \
        } \
    osi_Macro_End

#define osi_shlock_WrUnlock(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ReleaseWriteLock(&(x)->lock); \
        } \
    osi_Macro_End

#define osi_shlock_SToW(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            BoostSharedLock(&(x)->lock); \
        } \
    osi_Macro_End

#define osi_shlock_WToS(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            UnboostSharedLock(&(x)->lock); \
        } \
    osi_Macro_End

#define osi_shlock_WToR(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            ConvertWriteToReadLock(&(x)->lock); \
        } \
    osi_Macro_End

osi_extern int osi_shlock_NBRdLock(osi_shlock_t *);
osi_extern int osi_shlock_NBShLock(osi_shlock_t *);
osi_extern int osi_shlock_NBWrLock(osi_shlock_t *);

#endif /* _OSI_LWP_SHLOCK_H */
