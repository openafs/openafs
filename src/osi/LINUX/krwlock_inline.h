/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KRWLOCK_INLINE_H
#define	_OSI_LINUX_KRWLOCK_INLINE_H

osi_inline_define(
void
osi_rwlock_WrLock(osi_rwlock_t * lock)
{
    down_write(&lock->lock);
    lock->level = 1;
}
)
osi_inline_prototype(
void
osi_rwlock_WrLock(osi_rwlock_t * lock)
)

#if defined(OSI_IMPLEMENTS_RWLOCK_NBWRLOCK)
osi_inline_define(
int
osi_rwlock_NBWrLock(osi_rwlock_t * lock)
{
    int code;

    code = down_write_trylock(&lock->lock);
    if (code) {
        lock->level = 1;
    }

    return code;
}
)
osi_inline_prototype(
int
osi_rwlock_NBWrLock(osi_rwlock_t * lock)
)
#endif /* OSI_IMPLEMENTS_RWLOCK_NBWRLOCK */

osi_inline_define(
void
osi_rwlock_Unlock(osi_rwlock_t * lock)
{
    if (lock->level) {
	lock->level = 0;
	up_write(&lock->lock);
    } else {
	up_read(&lock->lock);
    }
}
)
osi_inline_prototype(
void
osi_rwlock_Unlock(osi_rwlock_t * lock)
)

#if defined(OSI_IMPLEMENTS_RWLOCK_DOWNGRADE)
osi_inline_define(
void
osi_rwlock_Downgrade(osi_rwlock_t * lock)
{
    lock->level = 0;
    downgrade_write(&lock->lock);
}
)
osi_inline_prototype(
void
osi_rwlock_Downgrade(osi_rwlock_t * lock)
)
#endif /* OSI_IMPLEMENTS_RWLOCK_DOWNGRADE */


#endif /* _OSI_LINUX_KRWLOCK_INLINE_H */
