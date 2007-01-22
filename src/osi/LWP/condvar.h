/*
 * Copyright 2005-2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LWP_CONDVAR_H
#define	_OSI_LWP_CONDVAR_H

#define OSI_IMPLEMENTS_CONDVAR 1

#include <lwp/lwp.h>
#include <lwp/lock.h>

typedef struct osi_condvar {
    char cv;
    osi_condvar_options_t opts;
} osi_condvar_t;

extern void osi_condvar_Init(osi_condvar_t *, osi_condvar_options_t *);
#define osi_condvar_Destroy(x)

#define osi_condvar_Signal(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            LWP_NoYieldSignal(&(x)->cv); \
        } \
    osi_Macro_End

#define osi_condvar_Broadcast(x) \
    osi_Macro_Begin \
        if (!(x)->opts.preemptive_only) { \
            LWP_NoYieldSignal(&(x)->cv); \
        } \
    osi_Macro_End

#define osi_condvar_Wait(c, l) \
    osi_Macro_Begin \
        if (!(c)->opts.preemptive_only) { \
            LWP_WaitProcess(&(c)->cv); \
        } \
    osi_Macro_End

#endif /* _OSI_LWP_CONDVAR_H */
