/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_PTHREAD_SHLOCK_H
#define	_OSI_PTHREAD_SHLOCK_H

#define OSI_IMPLEMENTS_SHLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_NBRDLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_NBSHLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_NBWRLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_WTOR 1

#include <lwp/lock.h>

typedef struct osi_shlock {
    struct Lock lock;
    osi_shlock_options_t opts;
} osi_shlock_t;

osi_extern void osi_shlock_Init(osi_shlock_t *, osi_shlock_options_t *);
osi_extern void osi_shlock_Destroy(osi_shlock_t *);

#define osi_shlock_RdLock(x) ObtainReadLock(&((x)->lock))
#define osi_shlock_ShLock(x) ObtainSharedLock(&((x)->lock))
#define osi_shlock_WrLock(x) ObtainWriteLock(&((x)->lock))
#define osi_shlock_RdUnlock(x) ReleaseReadLock(&((x)->lock))
#define osi_shlock_ShUnlock(x) ReleaseSharedLock(&((x)->lock))
#define osi_shlock_WrUnlock(x) ReleaseWriteLock(&((x)->lock))
#define osi_shlock_SToW(x) BoostSharedLock(&((x)->lock))
#define osi_shlock_WToS(x) UnboostSharedLock(&((x)->lock))
#define osi_shlock_WToR(x) ConvertWriteToReadLock(&((x)->lock))

osi_extern int osi_shlock_NBRdLock(osi_shlock_t *);
osi_extern int osi_shlock_NBShLock(osi_shlock_t *);
osi_extern int osi_shlock_NBWrLock(osi_shlock_t *);

#endif /* _OSI_PTHREAD_SHLOCK_H */
