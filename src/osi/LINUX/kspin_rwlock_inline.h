/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LINUX_KSPIN_RWLOCK_INLINE_H
#define _OSI_LINUX_KSPIN_RWLOCK_INLINE_H 1

osi_inline_define(
void
osi_spin_rwlock_WrLock(osi_spin_rwlock_t * lock)
{
    write_lock(&lock->lock);
    lock->level = 1;
}
)
osi_inline_prototype(
void
osi_spin_rwlock_WrLock(osi_spin_rwlock_t * lock)
)

#if defined(OSI_IMPLEMENTS_SPIN_RWLOCK_NBWRLOCK)
osi_inline_define(
int
osi_spin_rwlock_NBWrLock(osi_spin_rwlock_t * lock)
{
    int code;

    code = write_trylock(&lock->lock);
    if (code) {
        lock->level = 1;
    }

    return code;
}
)
osi_inline_prototype(
int
osi_spin_rwlock_NBWrLock(osi_spin_rwlock_t * lock)
)
#endif /* OSI_IMPLEMENTS_SPIN_RWLOCK_NBWRLOCK */

osi_inline_define(
void
osi_spin_rwlock_Unlock(osi_spin_rwlock_t * lock)
{
    if (lock->level) {
	lock->level = 0;
	write_unlock(&lock->lock);
    } else {
	read_unlock(&lock->lock);
    }
}
)
osi_inline_prototype(
void
osi_spin_rwlock_Unlock(osi_spin_rwlock_t * lock)
)


#endif /* _OSI_LINUX_KSPIN_RWLOCK_INLINE_H */
