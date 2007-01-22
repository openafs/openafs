/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_PTHREAD_RWLOCK_H
#define	_OSI_PTHREAD_RWLOCK_H

#define OSI_IMPLEMENTS_RWLOCK 1
#define OSI_IMPLEMENTS_RWLOCK_NBRDLOCK 1
#define OSI_IMPLEMENTS_RWLOCK_NBWRLOCK 1

#include <pthread.h>

typedef struct {
    pthread_rwlock_t lock;
    osi_rwlock_options_t opts;
} osi_rwlock_t;

#define osi_rwlock_Init(x, o) \
    osi_Macro_Begin \
        osi_Assert(pthread_rwlock_init(&((x)->lock), osi_NULL)==0); \
        osi_rwlock_options_Copy(&((x)->opts), (o)); \
    osi_Macro_End
#define osi_rwlock_Destroy(x) osi_Assert(pthread_rwlock_destroy(&((x)->lock))==0)
#define osi_rwlock_RdLock(x) osi_Assert(pthread_rwlock_rdlock(&((x)->lock))==0)
#define osi_rwlock_WrLock(x) osi_Assert(pthread_rwlock_wrlock(&((x)->lock))==0)
#define osi_rwlock_NBRdLock(x) (pthread_rwlock_tryrdlock(&((x)->lock))==0)
#define osi_rwlock_NBWrLock(x) (pthread_rwlock_trywrlock(&((x)->lock))==0)
#define osi_rwlock_Unlock(x) osi_Assert(pthread_rwlock_unlock(&((x)->lock))==0)

#endif /* _OSI_PTHREAD_RWLOCK_H */
