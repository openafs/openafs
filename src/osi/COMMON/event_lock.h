/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_EVENT_LOCK_H
#define _OSI_COMMON_EVENT_LOCK_H 1

/*
 * osi event action framework
 *
 * implementation using traditional synchronization primitives
 */

#include <osi/osi_list.h>
#include <osi/osi_mutex.h>
#include <osi/osi_rwlock.h>

/*
 * lock hierarchy rules:
 *
 * the set of subscription locks is higher in the lock hierarchy
 * than the set of hook locks
 *
 * you MUST NOT attempt to acquire an exclusive lock on a hook
 * while you are holding a subscription lock
 *
 * there is no defined ordering of subscription locks, therefore you
 * may only hold one subscription lock at a time
 *
 * there is no defined ordering of hook locks, therefore you
 * may only hold one hook lock at a time
 */

typedef enum {
    OSI_EVENT_HOOK_STATE_INVALID,
    OSI_EVENT_HOOK_STATE_ONLINE,
    OSI_EVENT_HOOK_STATE_PURGING,
    OSI_EVENT_HOOK_STATE_MAX_ID
} osi_event_hook_state_t;

typedef struct osi_event_hook {
    /*
     * BEGIN sync block
     * fields synchronized by hook->lock
     */
    /* list of subscriptions */
    osi_list_head_volatile watchers;

    /* opaque data */
    void * osi_volatile sdata;

    /* hook state */
    osi_event_hook_state_t state;

    /* number of threads waiting on state change */
    osi_uint32 state_waiters;
    /*
     * END sync block
     */

    osi_rwlock_t lock;
} osi_event_hook_t;

typedef struct osi_event_subscription {
    /*
     * BEGIN sync block
     * fields synchronized by either:
     *   subscription->lock && hook->lock held shared
     *   hook->lock held exclusive
     */
    /* linked list of subscriptions */
    osi_list_element_volatile watchers;
    /*
     * END sync block
     */

    /*
     * BEGIN sync block
     * the following fields are IMMUTABLE while subscribed
     * fields are synchronized by subscription->lock
     */
    /* action to perform when event fires */
    osi_event_action_t action;

    /* opaque data */
    void * osi_volatile sdata;
    /*
     * END sync block
     */

    /*
     * BEGIN sync block
     * fields synchronized by subscription->lock
     */
    /* pointer to the hook with which we are subscribed */
    osi_event_hook_t * osi_volatile hook;
    /*
     * END sync block
     */
    osi_mutex_t lock;
} osi_event_subscription_t;


#endif /* _OSI_COMMON_EVENT_LOCK_H */
