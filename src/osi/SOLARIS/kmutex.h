/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KMUTEX_H
#define	_OSI_SOLARIS_KMUTEX_H

#define OSI_IMPLEMENTS_MUTEX 1
#define OSI_IMPLEMENTS_MUTEX_NBLOCK 1
#define OSI_IMPLEMENTS_MUTEX_QUERY_LOCKHELD 1

#include <sys/mutex.h>

typedef struct {
    kmutex_t lock;
    osi_mutex_options_t opts;
} osi_mutex_t;

#define osi_mutex_Init(x, o) \
    osi_Macro_Begin \
        mutex_init(&((x)->lock), NULL, MUTEX_DEFAULT, NULL); \
        osi_mutex_options_Copy(&((x)->opts), (o)); \
    osi_Macro_End
#define osi_mutex_Destroy(x) mutex_destroy(&((x)->lock))
#define osi_mutex_Lock(x) mutex_enter(&((x)->lock))
#define osi_mutex_NBLock(x) mutex_tryenter(&((x)->lock))
#define osi_mutex_Unlock(x) mutex_exit(&((x)->lock))
#define osi_mutex_IsLockHeld(x) MUTEX_HELD(&((x)->lock))
#define osi_mutex_AssertLockHeld(x) osi_Assert(MUTEX_HELD(&((x)->lock)))

#endif /* _OSI_SOLARIS_KMUTEX_H */
