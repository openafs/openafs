/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_PTHREAD_H
#define AFS_PTHREAD_H

#include <windows.h>
#include <time.h>
#include <afs/errmap_nt.h>
#include <rx/rx_queue.h>

typedef struct {
    int call_started;
    int call_running;
} pthread_once_t;

#define PTHREAD_CREATE_JOINABLE 1
#define PTHREAD_CREATE_DETACHED 0

typedef struct {
    int is_joinable;
} pthread_attr_t;

#define PTHREAD_KEYS_MAX 32

typedef int pthread_condattr_t;
typedef int pthread_mutexattr_t;
typedef int pthread_key_t;
typedef void *pthread_t;

typedef struct  timespec {
    time_t          tv_sec;
    long            tv_nsec;
} timespec_t;


typedef struct {
    DWORD tid;
    int isLocked;
    CRITICAL_SECTION cs;
} pthread_mutex_t;

typedef struct cond_waiter {
    struct rx_queue wait_queue;
    HANDLE event;
} cond_waiters_t;
 
typedef struct {
    CRITICAL_SECTION cs;
    struct rx_queue waiting_threads;
} pthread_cond_t;

#define PTHREAD_ONCE_INIT {0,1}

extern int pthread_cond_broadcast(pthread_cond_t *cond);
extern int pthread_cond_destroy(pthread_cond_t *cond);
extern int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
extern int pthread_cond_signal(pthread_cond_t *cond);
extern int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
extern int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
extern int pthread_create(pthread_t *tid, const pthread_attr_t *attr, void *(*func)(void *), void *arg);
extern void *pthread_getspecific(pthread_key_t key);
extern int pthread_join(pthread_t target_thread, void **status);
extern int pthread_key_create(pthread_key_t *keyp, void (*destructor)(void *value));
extern int pthread_key_delete(pthread_key_t key);
extern int pthread_mutex_destroy(pthread_mutex_t *mp);
extern int pthread_mutex_init(pthread_mutex_t *mp, const pthread_mutexattr_t *attr);
extern int pthread_mutex_lock(pthread_mutex_t *mp);
extern int pthread_mutex_trylock(pthread_mutex_t *mp);
extern int pthread_mutex_unlock(pthread_mutex_t *mp);
extern int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));
extern int pthread_setspecific(pthread_key_t key, const void *value);
extern pthread_t pthread_self(void);
extern int pthread_equal(pthread_t t1, pthread_t t2);
extern int pthread_attr_destroy(pthread_attr_t *attr);
extern int pthread_attr_init(pthread_attr_t *attr);
extern int pthread_attr_getdetachstate(pthread_attr_t *attr, int *detachstate);
extern int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
extern void pthread_exit(void *status);

#endif
