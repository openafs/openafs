/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KRWLOCK_H
#define	_OSI_SOLARIS_KRWLOCK_H

#define OSI_IMPLEMENTS_RWLOCK 1
#define OSI_IMPLEMENTS_RWLOCK_NBRDLOCK 1
#define OSI_IMPLEMENTS_RWLOCK_NBWRLOCK 1
#define OSI_IMPLEMENTS_RWLOCK_DOWNGRADE 1
#define OSI_IMPLEMENTS_RWLOCK_TRYUPGRADE 1
#define OSI_IMPLEMENTS_RWLOCK_QUERY_WRLOCKHELD 1

#include <sys/rwlock.h>

typedef struct {
    krwlock_t lock;
    osi_rwlock_options_t opts;
} osi_rwlock_t;

#define osi_rwlock_Init(x, o) \
    osi_Macro_Begin \
        rw_init(&((x)->lock), NULL, RW_DEFAULT, NULL); \
        osi_rwlock_options_Copy(&((x)->opts), (o)); \
    osi_Macro_End
#define osi_rwlock_Destroy(x) rw_destroy(&((x)->lock))
#define osi_rwlock_RdLock(x) rw_enter(&((x)->lock), RW_READER)
#define osi_rwlock_WrLock(x) rw_enter(&((x)->lock), RW_WRITER)
#define osi_rwlock_Unlock(x) rw_exit(&((x)->lock))
#define osi_rwlock_NBRdLock(x) rw_tryenter(&((x)->lock), RW_READER)
#define osi_rwlock_NBWrLock(x) rw_tryenter(&((x)->lock), RW_WRITER)
#define osi_rwlock_Downgrade(x) rw_downgrade(&((x)->lock))
#define osi_rwlock_TryUpgrade(x) rw_tryupgrade(&((x)->lock))
#define osi_rwlock_IsWrLockHeld(x) rw_write_held(&((x)->lock))
#define osi_rwlock_AssertWrLockHeld(x) osi_Assert(rw_write_held(&((x)->lock)))

#endif /* _OSI_SOLARIS_KRWLOCK_H */
