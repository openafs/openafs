/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KMUTEX_H
#define _OSI_LINUX_KMUTEX_H 1

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))

#define OSI_IMPLEMENTS_MUTEX 1
#define OSI_IMPLEMENTS_MUTEX_NBLOCK 1

#include <linux/mutex.h>

typedef struct {
    struct mutex lock;
    osi_mutex_options_t opts;
} osi_mutex_t;

osi_extern void osi_mutex_Init(osi_mutex_t *, osi_mutex_options_t *);
#define osi_mutex_Destroy(x) mutex_destroy(&((x)->lock))
#define osi_mutex_Lock(x) mutex_lock(&((x)->lock))
#define osi_mutex_NBLock(x) mutex_trylock(&((x)->lock))
#define osi_mutex_Unlock(x) mutex_unlock(&((x)->lock))

#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16) */

#define OSI_IMPLEMENTS_MUTEX 1
#define OSI_IMPLEMENTS_MUTEX_NBLOCK 1

#include <linux/semaphore.h>

typedef struct {
    struct semaphore lock;
    osi_mutex_options_t opts;
} osi_mutex_t;

osi_extern void osi_mutex_Init(osi_mutex_t *, osi_mutex_options_t *);
#define osi_mutex_Destroy(x)
#define osi_mutex_Lock(x) down(&((x)->lock))
#define osi_mutex_NBLock(x) down_trylock(&((x)->lock))
#define osi_mutex_Unlock(x) up(&((x)->lock))

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16) */

#endif /* _OSI_LINUX_KMUTEX_H */
