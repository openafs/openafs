/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_KSHLOCK_H
#define	_OSI_LEGACY_KSHLOCK_H

#define OSI_IMPLEMENTS_SHLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_NBSHLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_NBWRLOCK 1
#define OSI_IMPLEMENTS_SHLOCK_WTOR 1
#define OSI_IMPLEMENTS_SHLOCK_STOR 1

#include "afs/lock.h"

typedef struct osi_shlock {
    afs_rwlock_t lock;
    osi_shlock_options_t opts;
} osi_shlock_t;

osi_extern void osi_shlock_Init(osi_shlock_t *, osi_shlock_options_t *);
#define osi_shlock_Destroy(x)

#define osi_shlock_RdLock(x) \
    osi_Macro_Begin \
        int _osi_shlock_rdlock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_shlock_rdlock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ObtainReadLock(&(x)->lock); \
        if (!_osi_shlock_rdlock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_shlock_ShLock(x) \
    osi_Macro_Begin \
        int _osi_shlock_shlock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_shlock_shlock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ObtainSharedLock(&(x)->lock, 0); \
        if (!_osi_shlock_shlock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_shlock_WrLock(x) \
    osi_Macro_Begin \
        int _osi_shlock_wrlock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_shlock_wrlock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ObtainWriteLock(&(x)->lock, 0); \
        if (!_osi_shlock_wrlock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_shlock_RdUnlock(x) \
    osi_Macro_Begin \
        int _osi_shlock_rdunlock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_shlock_rdunlock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ReleaseReadLock(&(x)->lock); \
        if (!_osi_shlock_rdunlock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_shlock_ShUnlock(x) \
    osi_Macro_Begin \
        int _osi_shlock_shunlock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_shlock_shunlock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ReleaseSharedLock(&(x)->lock); \
        if (!_osi_shlock_shunlock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_shlock_WrUnlock(x) \
    osi_Macro_Begin \
        int _osi_shlock_wrunlock_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_shlock_wrunlock_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ReleaseWriteLock(&(x)->lock); \
        if (!_osi_shlock_wrunlock_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_shlock_SToW(x) \
    osi_Macro_Begin \
        int _osi_shlock_stow_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_shlock_stow_haveGlock) { \
            AFS_GLOCK(); \
        } \
        UpgradeSToWLock(&(x)->lock, 0); \
        if (!_osi_shlock_stow_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_shlock_WToS(x) \
    osi_Macro_Begin \
        int _osi_shlock_wtos_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_shlock_wtos_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ConvertWToSLock(&(x)->lock); \
        if (!_osi_shlock_wtos_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_shlock_WToR(x) \
    osi_Macro_Begin \
        int _osi_shlock_wtor_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_shlock_wtor_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ConvertWToRLock(&(x)->lock); \
        if (!_osi_shlock_wtor_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

#define osi_shlock_SToR(x) \
    osi_Macro_Begin \
        int _osi_shlock_stor_haveGlock = ISAFS_GLOCK(); \
        if (!_osi_shlock_stor_haveGlock) { \
            AFS_GLOCK(); \
        } \
        ConvertSToRLock(&(x)->lock); \
        if (!_osi_shlock_stor_haveGlock) { \
            AFS_GUNLOCK(); \
        } \
    osi_Macro_End

osi_extern int osi_shlock_NBRdLock(osi_shlock_t *);
osi_extern int osi_shlock_NBShLock(osi_shlock_t *);
osi_extern int osi_shlock_NBWrLock(osi_shlock_t *);

#endif /* _OSI_LEGACY_KSHLOCK_H */
