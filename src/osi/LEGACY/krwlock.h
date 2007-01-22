/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_KRWLOCK_H
#define	_OSI_LEGACY_KRWLOCK_H

#define OSI_IMPLEMENTS_RWLOCK 1
#define OSI_IMPLEMENTS_RWLOCK_NBWRLOCK 1

#include "afs/lock.h"

typedef struct osi_rwlock {
    afs_rwlock_t lock;
    int level;
    osi_rwlock_options_t opts;
} osi_rwlock_t;

osi_extern void osi_rwlock_Init(osi_rwlock_t *, osi_rwlock_options_t *);
#define osi_rwlock_Destroy(x)

#define osi_rwlock_RdLock(x) \
    osi_Macro_Begin \
        int _osi_rwlock_rdlock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_rwlock_rdlock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ObtainReadLock(&(x)->lock); \
        (x)->level++; \
        if (!_osi_rwlock_rdlock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_rwlock_WrLock(x) \
    osi_Macro_Begin \
        int _osi_rwlock_wrlock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_rwlock_wrlock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ObtainWriteLock(&(x)->lock, 0); \
        (x)->level = -1; \
        if (!_osi_rwlock_wrlock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_rwlock_Unlock(x) \
    osi_Macro_Begin \
        int _osi_rwlock_unlock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_rwlock_unlock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        if ((x)->level == -1) { \
            (x)->level = 0; \
            ReleaseWriteLock(&(x)->lock); \
	} else { \
            (x)->level--; \
	    ReleaseReadLock(&(x)->lock); \
        } \
        if (!_osi_rwlock_unlock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End


osi_extern int osi_rwlock_NBWrLock(osi_rwlock_t *);


#endif /* _OSI_LEGACY_KRWLOCK_H */
