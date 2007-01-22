/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_PTHREAD_MUTEX_H
#define	_OSI_PTHREAD_MUTEX_H


#define OSI_IMPLEMENTS_MUTEX 1
#define OSI_IMPLEMENTS_MUTEX_NBLOCK 1

#include <pthread.h>

typedef struct {
    pthread_mutex_t lock;
    osi_mutex_options_t opts;
} osi_mutex_t;

#define osi_mutex_Init(x, o) \
    osi_Macro_Begin \
        osi_Assert(pthread_mutex_init(&((x)->lock), osi_NULL)==0); \
        osi_mutex_options_Copy(&((x)->opts), (o)); \
    osi_Macro_End
#define osi_mutex_Destroy(x) osi_Assert(pthread_mutex_destroy(&((x)->lock))==0)
#define osi_mutex_Lock(x) osi_Assert(pthread_mutex_lock(&((x)->lock))==0)
#define osi_mutex_NBLock(x) (pthread_mutex_trylock(&((x)->lock))==0)
#define osi_mutex_Unlock(x) osi_Assert(pthread_mutex_unlock(&((x)->lock))==0)

#endif /* _OSI_PTHREAD_MUTEX_H */
