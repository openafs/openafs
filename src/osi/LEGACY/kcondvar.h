/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_CONDVAR_H
#define	_OSI_LEGACY_CONDVAR_H

#define OSI_IMPLEMENTS_CONDVAR 1
#define OSI_IMPLEMENTS_CONDVAR_WAIT_SIG 1

#include <osi/osi_mutex.h>

typedef struct osi_condvar {
    osi_condvar_options_t opts;
} osi_condvar_t;

#define osi_condvar_Init(l, o) \
    osi_Macro_Begin \
        osi_condvar_options_Copy(&((l)->opts), (o)); \
    osi_Macro_End
#define osi_condvar_Destroy(x)

#define osi_condvar_Signal(x) \
    osi_Macro_Begin \
        int _osi_condvar_signal_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_condvar_signal_haveGlock) { \
            AFS_GLOCK(); \
        } \
        afs_osi_Wakeup(x); \
        if (!_osi_condvar_signal_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_condvar_Broadcast(x) \
    osi_Macro_Begin \
        int _osi_condvar_broadcast_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_condvar_broadcast_haveGlock) { \
            AFS_GLOCK(); \
        } \
        afs_osi_Wakeup(x); \
        if (!_osi_condvar_broadcast_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_condvar_Wait(c, l) \
    osi_Macro_Begin \
        int _osi_condvar_wait_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_condvar_wait_haveGlock) { \
            AFS_GLOCK(); \
        } \
        osi_mutex_Unlock(l); \
        afs_osi_Sleep((c)); \
        if (!_osi_condvar_wait_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
        osi_mutex_Lock(l); \
    osi_Macro_End

osi_extern int osi_condvar_WaitSig(osi_condvar_t *, osi_mutex_t *);


#endif /* _OSI_LEGACY_CONDVAR_H */
