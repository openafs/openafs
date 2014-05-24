/*
 * Copyright 2005-2008, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFS_VOL_VOLUME_INLINE_H
#define _AFS_VOL_VOLUME_INLINE_H 1

#include "volume.h"
#include "partition.h"

#ifdef AFS_DEMAND_ATTACH_FS
# include "lock.h"
#endif

#ifdef AFS_PTHREAD_ENV
/**
 * @param[in] cv cond var
 * @param[in] ts deadline, or NULL to wait forever
 * @param[out] timedout  set to 1 if we returned due to the deadline, 0 if we
 *                       returned due to the cond var getting signalled. If
 *                       NULL, it is ignored.
 */
static_inline void
VOL_CV_TIMEDWAIT(pthread_cond_t *cv, const struct timespec *ts, int *timedout)
{
    int code;
    if (timedout) {
	*timedout = 0;
    }
    if (!ts) {
	VOL_CV_WAIT(cv);
	return;
    }
    VOL_LOCK_DBG_CV_WAIT_BEGIN;
    code = CV_TIMEDWAIT(cv, &vol_glock_mutex, ts);
    VOL_LOCK_DBG_CV_WAIT_END;
    if (code == ETIMEDOUT) {
	code = 0;
	if (timedout) {
	    *timedout = 1;
	}
    }
    osi_Assert(code == 0);
}
#endif /* AFS_PTHREAD_ENV */

/**
 * tell caller whether the given program type represents a salvaging
 * program.
 *
 * @param type  program type enumeration
 *
 * @return whether program state is a salvager
 *   @retval 0  type is a non-salvaging program
 *   @retval 1  type is a salvaging program
 */
static_inline int
VIsSalvager(ProgramType type)
{
    switch(type) {
    case salvager:
    case salvageServer:
    case volumeSalvager:
	return 1;
    default:
	return 0;
    }
}

/**
 * tells caller whether or not we need to lock the entire partition when
 * attaching a volume.
 *
 * @return whether or not we need to lock the partition
 *  @retval 0  no, we do not
 *  @retval 1  yes, we do
 *
 * @note for DAFS, always returns 0, since we use per-header locks instead
 */
static_inline int
VRequiresPartLock(void)
{
#ifdef AFS_DEMAND_ATTACH_FS
    return 0;
#else
    switch (programType) {
    case volumeServer:
    case volumeUtility:
        return 1;
    default:
        return 0;
    }
#endif /* AFS_DEMAND_ATTACH_FS */
}

/**
 * tells caller whether or not we need to check out a volume from the
 * fileserver before we can use it.
 *
 * @param[in] mode the mode of attachment for the volume
 *
 * @return whether or not we need to check out the volume from the fileserver
 *  @retval 0 no, we can just use the volume
 *  @retval 1 yes, we must check out the volume before use
 */
static_inline int
VMustCheckoutVolume(int mode)
{
    if (VCanUseFSSYNC() && mode != V_SECRETLY && mode != V_PEEK) {
	return 1;
    }
    return 0;
}

/**
 * tells caller whether we should check the inUse field in the volume
 * header when attaching a volume.
 *
 * If we check inUse, that generally means we will salvage the volume
 * (or put it in an error state) if we detect that another program
 * claims to be using the volume when we try to attach. We don't always
 * want to do that, since sometimes we know that the volume may be in
 * use by another program, e.g. when we are attaching with V_PEEK
 * or attaching for only reading (V_READONLY).
 *
 * @param mode  the mode of attachment for the volume
 *
 * @return whether or not we should check inUse
 *  @retval 0  no, we should not check inUse
 *  @retval 1  yes, we should check inUse
 */
static_inline int
VShouldCheckInUse(int mode)
{
    if (VCanUnsafeAttach()) {
	return 0;
    }
    if (programType == fileServer) {
       return 1;
    }
    if (VMustCheckoutVolume(mode)) {
	/*
	 * Before VShouldCheckInUse() was called, the caller checked out the
	 * volume from the fileserver. The volume may not be in use by the
	 * fileserver, or another program, at this point. The caller should
	 * verify by checking inUse is not set, otherwise the volume state
	 * is in error.
	 *
	 * However, an exception is made for the V_READONLY attach mode.  The
	 * volume may still be in use by the fileserver when a caller has
	 * checked out the volume from the fileserver with the V_READONLY
	 * attach mode, and so it is not an error for the inUse field to be set
	 * at this point. The caller should not check the inUse and may
	 * not change any volume state.
	 */
	if (mode == V_READONLY) {
	    return 0; /* allowed to be inUse; do not check */
	}
	return 1; /* may not be inUse; check */
    }
    return 0;
}

#ifdef AFS_DEMAND_ATTACH_FS
/**
 * acquire a non-blocking disk lock for a particular volume id.
 *
 * @param[in] volid the volume ID to lock
 * @param[in] dp    the partition on which 'volid' resides
 * @param[in] locktype READ_LOCK or WRITE_LOCK
 *
 * @return operation status
 *  @retval 0 success, lock was obtained
 *  @retval EBUSY another process holds a conflicting lock
 *  @retval EIO   error acquiring lock
 *
 * @note Use VLockVolumeNB instead, if possible; only use this directly if
 * you are not dealing with 'Volume*'s and attached volumes and such
 *
 * @pre There must not be any other threads acquiring locks on the same volid
 * and partition; the locks will not work correctly if two threads try to
 * acquire locks for the same volume
 */
static_inline int
VLockVolumeByIdNB(VolumeId volid, struct DiskPartition64 *dp, int locktype)
{
    return VLockFileLock(&dp->volLockFile, volid, locktype, 1 /* nonblock */);
}

/**
 * release a lock acquired by VLockVolumeByIdNB.
 *
 * @param[in] volid the volume id to unlock
 * @param[in] dp    the partition on which 'volid' resides
 *
 * @pre volid was previously locked by VLockVolumeByIdNB
 */
static_inline void
VUnlockVolumeById(VolumeId volid, struct DiskPartition64 *dp)
{
    VLockFileUnlock(&dp->volLockFile, volid);
}

/***************************************************/
/* demand attach fs state machine routines         */
/***************************************************/

/**
 * tells caller whether we need to keep volumes locked for the entire time we
 * are using them, or if we can unlock volumes as soon as they are attached.
 *
 * @return whether we can unlock attached volumes or not
 *  @retval 1 yes, we can unlock attached volumes
 *  @retval 0 no, do not unlock volumes until we unattach them
 */
static_inline int
VCanUnlockAttached(void)
{
    switch(programType) {
    case fileServer:
	return 1;
    default:
	return 0;
    }
}

/**
 * tells caller whether we need to lock a vol header with a write lock, a
 * read lock, or if we do not need to lock it at all, when attaching.
 *
 * @param[in]  mode  volume attachment mode
 * @param[in]  writeable  1 if the volume is writable, 0 if not
 *
 * @return how we need to lock the vol header
 *  @retval 0 do not lock the vol header at all
 *  @retval READ_LOCK lock the vol header with a read lock
 *  @retval WRITE_LOCK lock the vol header with a write lock
 *
 * @note DAFS only (non-DAFS uses partition locks)
 */
static_inline int
VVolLockType(int mode, int writeable)
{
    switch (programType) {
    case fileServer:
	if (writeable) {
	    return WRITE_LOCK;
	}
	return READ_LOCK;

    case volumeSalvager:
    case salvageServer:
    case salvager:
	return WRITE_LOCK;

    default:
	/* volserver, vol utilies, etc */

	switch (mode) {
	case V_READONLY:
	    return READ_LOCK;

	case V_VOLUPD:
	case V_SECRETLY:
	    return WRITE_LOCK;

	case V_CLONE:
	case V_DUMP:
	    if (writeable) {
		return WRITE_LOCK;
	    }
	    return READ_LOCK;

	case V_PEEK:
	    return 0;

	default:
	    osi_Assert(0 /* unknown checkout mode */);
	    return 0;
	}
    }
}

/**
 * tells caller whether or not the volume is effectively salvaging.
 *
 * @param vp  volume pointer
 *
 * @return whether volume is salvaging or not
 *  @retval 0 no, volume is not salvaging
 *  @retval 1 yes, volume is salvaging
 *
 * @note The volume may not actually be getting salvaged at the moment if
 *       this returns 1, but may have just been requested or scheduled to be
 *       salvaged. Callers should treat these cases as pretty much the same
 *       anyway, since we should not touch a volume that is busy salvaging or
 *       waiting to be salvaged.
 */
static_inline int
VIsSalvaging(struct Volume *vp)
{
    /* these tests are a bit redundant, but to be safe... */
    switch(V_attachState(vp)) {
    case VOL_STATE_SALVAGING:
    case VOL_STATE_SALVAGE_REQ:
	return 1;
    default:
	if (vp->salvage.requested || vp->salvage.scheduled) {
	    return 1;
	}
    }
    return 0;
}

/**
 * tells caller whether or not the current state requires
 * exclusive access without holding glock.
 *
 * @param state  volume state enumeration
 *
 * @return whether volume state is a mutually exclusive state
 *   @retval 0  no, state is re-entrant
 *   @retval 1  yes, state is mutually exclusive
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline int
VIsExclusiveState(VolState state)
{
    switch (state) {
    case VOL_STATE_UPDATING:
    case VOL_STATE_ATTACHING:
    case VOL_STATE_GET_BITMAP:
    case VOL_STATE_HDR_LOADING:
    case VOL_STATE_HDR_ATTACHING:
    case VOL_STATE_OFFLINING:
    case VOL_STATE_DETACHING:
    case VOL_STATE_SALVSYNC_REQ:
    case VOL_STATE_VNODE_ALLOC:
    case VOL_STATE_VNODE_GET:
    case VOL_STATE_VNODE_CLOSE:
    case VOL_STATE_VNODE_RELEASE:
    case VOL_STATE_VLRU_ADD:
    case VOL_STATE_SCANNING_RXCALLS:
	return 1;
    default:
	return 0;
    }
}

/**
 * tell caller whether V_attachState is an error condition.
 *
 * @param state  volume state enumeration
 *
 * @return whether volume state is in error state
 *   @retval 0  state is not an error state
 *   @retval 1  state is an error state
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline int
VIsErrorState(VolState state)
{
    switch (state) {
    case VOL_STATE_ERROR:
    case VOL_STATE_SALVAGING:
    case VOL_STATE_SALVAGE_REQ:
	return 1;
    default:
	return 0;
    }
}

/**
 * tell caller whether V_attachState is an offline condition.
 *
 * @param state  volume state enumeration
 *
 * @return whether volume state is in offline state
 *   @retval 0  state is not an offline state
 *   @retval 1  state is an offline state
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline int
VIsOfflineState(VolState state)
{
    switch (state) {
    case VOL_STATE_UNATTACHED:
    case VOL_STATE_ERROR:
    case VOL_STATE_SALVAGING:
    case VOL_STATE_DELETED:
	return 1;
    default:
	return 0;
    }
}

/**
 * tell caller whether V_attachState is valid.
 *
 * @param state  volume state enumeration
 *
 * @return whether volume state is a mutually exclusive state
 *   @retval 0  no, state is not valid
 *   @retval 1  yes, state is a valid enumeration member
 *
 * @note DEMAND_ATTACH_FS only
 *
 * @note do we really want to treat VOL_STATE_FREED as valid?
 */
static_inline int
VIsValidState(VolState state)
{
    if (((int) state >= 0) &&
	(state < VOL_STATE_COUNT)) {
	return 1;
    }
    return 0;
}

/**
 * increment volume-package internal refcount.
 *
 * @param vp  volume object pointer
 *
 * @internal volume package internal use only
 *
 * @pre VOL_LOCK must be held
 *
 * @post volume waiters refcount is incremented
 *
 * @see VCancelReservation_r
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline void
VCreateReservation_r(Volume * vp)
{
    vp->nWaiters++;
}

/**
 * wait for the volume to change states.
 *
 * @param vp  volume object pointer
 *
 * @pre VOL_LOCK held; ref held on volume
 *
 * @post VOL_LOCK held; volume state has changed from previous value
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline void
VWaitStateChange_r(Volume * vp)
{
    VolState state_save = V_attachState(vp);

    osi_Assert(vp->nWaiters || vp->nUsers);
    do {
	VOL_CV_WAIT(&V_attachCV(vp));
    } while (V_attachState(vp) == state_save);
    osi_Assert(V_attachState(vp) != VOL_STATE_FREED);
}

/**
 * wait for the volume to change states within a certain amount of time
 *
 * @param[in] vp  volume object pointer
 * @param[in] ts  deadline (absolute time) or NULL to wait forever
 *
 * @pre VOL_LOCK held; ref held on volume
 * @post VOL_LOCK held; volume state has changed and/or it is after the time
 *       specified in ts
 *
 * @note DEMAND_ATTACH_FS only
 * @note if ts is NULL, this is identical to VWaitStateChange_r
 */
static_inline void
VTimedWaitStateChange_r(Volume * vp, const struct timespec *ts, int *atimedout)
{
    VolState state_save;
    int timeout;

    if (atimedout) {
	*atimedout = 0;
    }

    if (!ts) {
	VWaitStateChange_r(vp);
	return;
    }

    state_save = V_attachState(vp);

    osi_Assert(vp->nWaiters || vp->nUsers);
    do {
	VOL_CV_TIMEDWAIT(&V_attachCV(vp), ts, &timeout);
    } while (V_attachState(vp) == state_save && !timeout);
    osi_Assert(V_attachState(vp) != VOL_STATE_FREED);

    if (atimedout && timeout) {
	*atimedout = 1;
    }
}

/**
 * wait for blocking ops to end.
 *
 * @pre VOL_LOCK held; ref held on volume
 *
 * @post VOL_LOCK held; volume not in exclusive state
 *
 * @param vp  volume object pointer
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline void
VWaitExclusiveState_r(Volume * vp)
{
    osi_Assert(vp->nWaiters || vp->nUsers);
    while (VIsExclusiveState(V_attachState(vp))) {
	VOL_CV_WAIT(&V_attachCV(vp));
    }
    osi_Assert(V_attachState(vp) != VOL_STATE_FREED);
}

/**
 * change state, and notify other threads,
 * return previous state to caller.
 *
 * @param vp         pointer to volume object
 * @param new_state  new volume state value
 * @pre VOL_LOCK held
 *
 * @post volume state changed; stats updated
 *
 * @return previous volume state
 *
 * @note DEMAND_ATTACH_FS only
 */
static_inline VolState
VChangeState_r(Volume * vp, VolState new_state)
{
    VolState old_state = V_attachState(vp);

    /* XXX profiling need to make sure these counters
     * don't kill performance... */
    VStats.state_levels[old_state]--;
    VStats.state_levels[new_state]++;

    V_attachState(vp) = new_state;
    CV_BROADCAST(&V_attachCV(vp));
    return old_state;
}

#endif /* AFS_DEMAND_ATTACH_FS */

#define VENUMCASE(en) \
    case en: return #en

/**
 * translate a ProgramType code to a string.
 *
 * @param[in] type ProgramType numeric code
 *
 * @return a human-readable string for that program type
 *  @retval "**UNKNOWN**" an unknown ProgramType was given
 */
static_inline char *
VPTypeToString(ProgramType type)
{
    switch (type) {
	VENUMCASE(fileServer);
	VENUMCASE(volumeUtility);
	VENUMCASE(salvager);
	VENUMCASE(salvageServer);
	VENUMCASE(debugUtility);
	VENUMCASE(volumeServer);
	VENUMCASE(volumeSalvager);
    default:
	return "**UNKNOWN**";
    }
}

#undef VENUMCASE

#endif /* _AFS_VOL_VOLUME_INLINE_H */
