/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFS_PTHREAD_GLOCK_H_
#define _AFS_PTHREAD_GLOCK_H_

#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#include <assert.h>

typedef struct {
    pthread_mutex_t mut;
    volatile pthread_t owner;
    volatile unsigned int locked;
    volatile unsigned int times_inside;
} pthread_recursive_mutex_t, *pthread_recursive_mutex_p;

#if defined(AFS_NT40_ENV) && defined(AFS_PTHREAD_ENV)
#ifndef AFS_GRMUTEX_DECLSPEC
#define AFS_GRMUTEX_DECLSPEC __declspec(dllimport) extern
#endif
#else
#define AFS_GRMUTEX_DECLSPEC extern
#endif

AFS_GRMUTEX_DECLSPEC pthread_recursive_mutex_t grmutex;

extern int pthread_recursive_mutex_lock(pthread_recursive_mutex_p);
extern int pthread_recursive_mutex_unlock(pthread_recursive_mutex_p);

#define LOCK_GLOBAL_MUTEX \
    assert(pthread_recursive_mutex_lock(&grmutex)==0)

#define UNLOCK_GLOBAL_MUTEX \
    assert(pthread_recursive_mutex_unlock(&grmutex)==0)

#else

#define LOCK_GLOBAL_MUTEX
#define UNLOCK_GLOBAL_MUTEX

#endif /* AFS_PTHREAD_ENV */

#endif /* _AFS_PTHREAD_GLOCK_H_ */
