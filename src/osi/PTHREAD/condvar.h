/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_PTHREAD_CONDVAR_H
#define	_OSI_PTHREAD_CONDVAR_H

#define OSI_IMPLEMENTS_CONDVAR 1

#include <pthread.h>

typedef struct {
    pthread_cond_t cv;
    osi_condvar_options_t opts;
} osi_condvar_t;

#define osi_condvar_Init(x, o) \
    osi_Macro_Begin \
        osi_Assert(pthread_cond_init(&((x)->cv), NULL)==0); \
        osi_condvar_options_Copy(&((x)->opts), (o)); \
    osi_Macro_End
#define osi_condvar_Destroy(x) osi_Assert(pthread_cond_destroy(&((x)->cv))==0)
#define osi_condvar_Signal(x) osi_Assert(pthread_cond_signal(&((x)->cv))==0)
#define osi_condvar_Broadcast(x) osi_Assert(pthread_cond_broadcast(&((x)->cv))==0)
#define osi_condvar_Wait(c, l) osi_Assert(pthread_cond_wait(&((c)->cv), &((l)->lock))==0)

#endif /* _OSI_PTHREAD_CONDVAR_H */
