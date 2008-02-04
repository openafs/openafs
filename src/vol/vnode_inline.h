/*
 * Copyright 2007-2008, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFS_VOL_VNODE_INLINE_H
#define _AFS_VOL_VNODE_INLINE_H 1

#include "vnode.h"

#ifdef AFS_AIX_ENV
#define static_inline inline
#else
#define static_inline static inline
#endif

/***************************************************/
/* demand attach vnode state machine routines      */
/***************************************************/

/**
 * get a reference to a vnode object.
 *
 * @param[in] vnp  vnode object pointer
 *
 * @internal vnode package internal use only
 *
 * @pre VOL_LOCK must be held
 *
 * @post vnode refcount incremented
 *
 * @see VnCancelReservation_r
 */
static_inline void
VnCreateReservation_r(Vnode * vnp)
{
    Vn_refcount(vnp)++;
    if (Vn_refcount(vnp) == 1) {
	DeleteFromVnLRU(Vn_class(vnp), vnp);
    }
}

extern int TrustVnodeCacheEntry;

/**
 * release a reference to a vnode object.
 *
 * @param[in] vnp  vnode object pointer
 *
 * @pre VOL_LOCK held
 *
 * @post refcount decremented; possibly re-added to vn lru
 *
 * @internal vnode package internal use only
 *
 * @see VnCreateReservation_r
 */
static_inline void
VnCancelReservation_r(Vnode * vnp)
{
    if (--Vn_refcount(vnp) == 0) {
	AddToVnLRU(Vn_class(vnp), vnp);

	/* If caching is turned off, 
	 * disassociate vnode cache entry from volume object */
	if (!TrustVnodeCacheEntry) {
	    DeleteFromVVnList(vnp);
	}
    }
}

#ifdef AFS_PTHREAD_ENV
#define VN_SET_WRITER_THREAD_ID(v)  (((v)->writer) = pthread_self())
#else
#define VN_SET_WRITER_THREAD_ID(v)  (LWP_CurrentProcess(&((v)->writer)))
#endif

#define VOL_LOCK_NOT_HELD 0
#define VOL_LOCK_HELD 1
#define MIGHT_DEADLOCK 0
#define WILL_NOT_DEADLOCK 1

/**
 * acquire a lock on a vnode object.
 *
 * @param[in] vnp   vnode object pointer
 * @param[in] type  lock type
 * @param[in] held  whether or not vol glock is held
 * @param[in] safe  whether it it is safe to acquire without dropping vol glock
 *
 * @note caller must guarantee deadlock will not occur
 *
 * @post lock acquired.
 *       for write case, thread owner field set.
 *
 * @note for DAFS, this is a no-op
 *
 * @internal vnode package internal use only
 */
static_inline void
VnLock(Vnode * vnp, int type, int held, int safe)
{
#ifdef AFS_DEMAND_ATTACH_FS
    if (type == WRITE_LOCK) {
	VN_SET_WRITER_THREAD_ID(vnp);
    }
#else /* !AFS_DEMAND_ATTACH_FS */
    if (held && !safe) {
	VOL_UNLOCK;
    }
    if (type == READ_LOCK) {
	ObtainReadLock(&vnp->lock);
    } else {
	ObtainWriteLock(&vnp->lock);
	VN_SET_WRITER_THREAD_ID(vnp);
    }
    if (held && !safe) {
	VOL_LOCK;
    }
#endif /* !AFS_DEMAND_ATTACH_FS */
}

/**
 * release a lock on a vnode object.
 *
 * @param[in] vnp   vnode object pointer
 * @param[in] type  lock type
 *
 * @note for DAFS, this is a no-op
 *
 * @internal vnode package internal use only
 */
static_inline void
VnUnlock(Vnode * vnp, int type)
{
    if (type == READ_LOCK) {
#ifndef AFS_DEMAND_ATTACH_FS
	ReleaseReadLock(&vnp->lock);
#endif
    } else {
	vnp->writer = 0;
#ifndef AFS_DEMAND_ATTACH_FS
	ReleaseWriteLock(&vnp->lock);
#endif
    }
}


#ifdef AFS_DEMAND_ATTACH_FS
/**
 * change state, and notify other threads,
 * return previous state to caller.
 *
 * @param[in] vnp        pointer to vnode object
 * @param[in] new_state  new vnode state value
 *
 * @pre VOL_LOCK held
 *
 * @post vnode state changed
 *
 * @return previous vnode state
 *
 * @note DEMAND_ATTACH_FS only
 *
 * @internal vnode package internal use only
 */
static_inline VnState
VnChangeState_r(Vnode * vnp, VnState new_state)
{
    VnState old_state = Vn_state(vnp);

    Vn_state(vnp) = new_state;
    assert(pthread_cond_broadcast(&Vn_stateCV(vnp)) == 0);
    return old_state;
}

/**
 * tells caller whether or not the current state requires
 * exclusive access without holding glock.
 *
 * @param[in] state  vnode state enumeration
 *
 * @return whether vnode state is a mutually exclusive state
 *   @retval 0  no, state is re-entrant
 *   @retval 1  yes, state is mutually exclusive
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline int
VnIsExclusiveState(VnState state)
{
    switch (state) {
    case VN_STATE_RELEASING:
    case VN_STATE_CLOSING:
    case VN_STATE_ALLOC:
    case VN_STATE_LOAD:
    case VN_STATE_EXCLUSIVE:
    case VN_STATE_STORE:
	return 1;
    }
    return 0;
}

/**
 * tell caller whether vnode state is an error condition.
 *
 * @param[in] state  vnode state enumeration
 *
 * @return whether vnode state is in error state
 *   @retval 0  state is not an error state
 *   @retval 1  state is an error state
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline int
VnIsErrorState(VnState state)
{
    switch (state) {
    case VN_STATE_ERROR:
	return 1;
    }
    return 0;
}

/**
 * tell caller whether vnode state is valid.
 *
 * @param[in] state  vnode state enumeration
 *
 * @return whether vnode state is a mutually exclusive state
 *   @retval 0  no, state is not valid
 *   @retval 1  yes, state is a valid enumeration member
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline int
VnIsValidState(VnState state)
{
    if ((state >= 0) && 
	(state < VN_STATE_COUNT)) {
	return 1;
    }
    return 0;
}

/**
 * wait for the vnode to change states.
 *
 * @param[in] vnp  vnode object pointer
 *
 * @pre VOL_LOCK held; ref held on vnode
 *
 * @post VOL_LOCK held; vnode state has changed from previous value
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline void
VnWaitStateChange_r(Vnode * vnp)
{
    VnState state_save = Vn_state(vnp);

    assert(Vn_refcount(vnp));
    do {
	VOL_CV_WAIT(&Vn_stateCV(vnp));
    } while (Vn_state(vnp) == state_save);
    assert(!(Vn_stateFlags(vnp) & VN_ON_LRU));
}

/**
 * wait for blocking ops to end.
 *
 * @pre VOL_LOCK held; ref held on vnode
 *
 * @post VOL_LOCK held; vnode not in exclusive state
 *
 * @param[in] vnp  vnode object pointer
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline void
VnWaitExclusiveState_r(Vnode * vnp)
{
    assert(Vn_refcount(vnp));
    while (VnIsExclusiveState(Vn_state(vnp))) {
	VOL_CV_WAIT(&Vn_stateCV(vnp));
    }
    assert(!(Vn_stateFlags(vnp) & VN_ON_LRU));
}

/**
 * wait until vnode is in non-exclusive state, and there are no active readers.
 *
 * @param[in] vnp  vnode object pointer
 *
 * @pre VOL_LOCK held; ref held on vnode
 *
 * @post VOL_LOCK held; vnode is in non-exclusive state and has no active readers
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline void
VnWaitQuiescent_r(Vnode * vnp)
{
    assert(Vn_refcount(vnp));
    while (VnIsExclusiveState(Vn_state(vnp)) ||
	   Vn_readers(vnp)) {
	VOL_CV_WAIT(&Vn_stateCV(vnp));
    }
    assert(!(Vn_stateFlags(vnp) & VN_ON_LRU));
}

/**
 * register a new reader on a vnode.
 *
 * @param[in] vnp  vnode object pointer
 *
 * @pre VOL_LOCK held.
 *      ref held on vnode.
 *      vnode in VN_STATE_READ or VN_STATE_ONLINE
 *
 * @post refcount incremented.
 *       state set to VN_STATE_READ.
 *
 * @note DEMAND_ATTACH_FS only
 *
 * @internal vnode package internal use only
 */
static_inline void
VnBeginRead_r(Vnode * vnp)
{
    if (!Vn_readers(vnp)) {
	assert(Vn_state(vnp) == VN_STATE_ONLINE);
	VnChangeState_r(vnp, VN_STATE_READ);
    }
    Vn_readers(vnp)++;
    assert(Vn_state(vnp) == VN_STATE_READ);
}

/**
 * deregister a reader on a vnode.
 *
 * @param[in] vnp  vnode object pointer
 *
 * @pre VOL_LOCK held.
 *      ref held on vnode.
 *      read ref held on vnode.
 *      vnode in VN_STATE_READ.
 *
 * @post refcount decremented.
 *       when count reaches zero, state set to VN_STATE_ONLINE.
 *
 * @note DEMAND_ATTACH_FS only
 *
 * @internal vnode package internal use only
 */
static_inline void
VnEndRead_r(Vnode * vnp)
{
    assert(Vn_readers(vnp) > 0);
    Vn_readers(vnp)--;
    if (!Vn_readers(vnp)) {
	assert(pthread_cond_broadcast(&Vn_stateCV(vnp)) == 0);
	VnChangeState_r(vnp, VN_STATE_ONLINE);
    }
}

#endif /* AFS_DEMAND_ATTACH_FS */

#endif /* _AFS_VOL_VNODE_INLINE_H */
