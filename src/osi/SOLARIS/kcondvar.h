/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KCONDVAR_H
#define	_OSI_SOLARIS_KCONDVAR_H

#define OSI_IMPLEMENTS_CONDVAR 1
#define OSI_IMPLEMENTS_CONDVAR_WAIT_SIG 1

#include <sys/condvar.h>

typedef struct {
    kcondvar_t cv;
    osi_condvar_options_t opts;
} osi_condvar_t;

#define osi_condvar_Init(x, o) \
    osi_Macro_Begin \
        cv_init(&((x)->cv), NULL, CV_DEFAULT, NULL); \
        osi_condvar_options_Copy(&((x)->opts), (o)); \
    osi_Macro_End
#define osi_condvar_Destroy(x) cv_destroy(&((x)->cv))
#define osi_condvar_Signal(x) cv_signal(&((x)->cv))
#define osi_condvar_Broadcast(x) cv_broadcast(&((x)->cv))
#define osi_condvar_Wait(c, l) cv_wait(&((c)->cv), &((l)->lock))
#define osi_condvar_WaitSig(c, l) ((cv_wait_sig(&((c)->cv), &((l)->lock))==0) ? EINTR : 0)

#endif /* _OSI_SOLARIS_KCONDVAR_H */
