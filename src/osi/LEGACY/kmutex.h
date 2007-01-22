/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_MUTEX_H
#define	_OSI_LEGACY_MUTEX_H

#define OSI_IMPLEMENTS_MUTEX 1
#define OSI_IMPLEMENTS_MUTEX_NBLOCK 1


typedef struct osi_mutex {
    afs_lock_t lock;
    osi_mutex_options_t opts;
} osi_mutex_t;

osi_extern void osi_mutex_Init(osi_mutex_t *, osi_mutex_options_t *);
#define osi_mutex_Destroy(x)

#define osi_mutex_Lock(x) \
    osi_Macro_Begin \
        int _osi_mutex_lock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_mutex_lock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ObtainWriteLock(&(x)->lock, 0); \
        if (!_osi_mutex_lock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_mutex_Unlock(x) \
    osi_Macro_Begin \
        int _osi_mutex_unlock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_mutex_unlock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ReleaseWriteLock(&(x)->lock); \
        if (!_osi_mutex_unlock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

osi_extern int osi_mutex_NBLock(osi_mutex_t *);

#endif /* _OSI_LEGACY_MUTEX_H */
