/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


#include <osi/osi_impl.h>
#include <osi/osi_event.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_mutex.h>
#include <osi/osi_condvar.h>
#include <osi/osi_object_cache.h>
#include <osi/COMMON/event_lock.h>
#include <osi/COMMON/event_types.h>

osi_static osi_mutex_t osi_event_hook_purge_lock;
osi_static osi_condvar_t osi_event_hook_purge_cv;

osi_static osi_mem_object_cache_t * osi_event_record_cache = osi_NULL;
osi_static osi_mem_object_cache_t * osi_event_action_cv_cache = osi_NULL;
osi_static osi_mem_object_cache_t * osi_event_action_queue_cache = osi_NULL;

/*
 * initialize an event hook object
 *
 * [IN] hook  -- pointer to hook object
 *
 * preconditions:
 *   hook is in an uninitialized state
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_event_hook_Init(osi_event_hook_t * hook)
{
    osi_result res = OSI_OK;

    osi_rwlock_Init(&hook->lock,
		    osi_impl_rwlock_opts());
    osi_list_Init(&hook->watchers);
    hook->sdata = osi_NULL;

    return res;
}

/*
 * destroy a hook object
 *
 * [IN] hook  -- pointer to hook object
 *
 * preconditions:
 *   hook was previously initialized
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_event_hook_Destroy(osi_event_hook_t * hook)
{
    osi_result res = OSI_OK;

    osi_rwlock_Destroy(&hook->lock);

    return res;
}

/*
 * sets the opaque data pointer in the hook object
 *
 * [IN] hook  -- pointer to hook object
 * [IN] rock  -- opaque pointer
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_event_hook_set_rock(osi_event_hook_t * hook,
			void * rock)
{
    osi_result res = OSI_OK;

    osi_rwlock_WrLock(&hook->lock);
    hook->sdata = rock;
    osi_rwlock_Unlock(&hook->lock);

    return res;
}

/*
 * clear all subscriptions from this event hook
 *
 * [IN] hook  -- pointer to hook object
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_event_hook_clear(osi_event_hook_t * hook)
{
    osi_result res = OSI_OK;
    osi_list_head clear_queue;
    osi_event_subscription_t * sub, * nsub;

    osi_list_Init(&clear_queue);

    osi_rwlock_WrLock(&hook->lock);

    switch (hook->state) {
    case OSI_EVENT_HOOK_STATE_PURGING:
	goto wait_race;
    case OSI_EVENT_HOOK_STATE_ONLINE:
	break;
    default:
	goto invalid_state;
    }

 race_over:
    osi_list_SpliceAppend(&clear_queue, &hook->watchers);
    hook->state = OSI_EVENT_HOOK_STATE_PURGING;
    osi_rwlock_Unlock(&hook->lock);

    for (osi_list_Scan(&clear_queue,
		       sub, nsub,
		       osi_event_subscription_t,
		       watchers)) {
	osi_mutex_Lock(&sub->lock);
	sub->hook = osi_NULL;
	osi_mutex_Unlock(&sub->lock);
    }

    osi_rwlock_WrLock(&hook->lock);
    if (hook->state_waiters) {
	osi_mutex_Lock(&osi_event_hook_purge_lock);
	osi_condvar_Signal(&osi_event_hook_purge_cv);
	osi_mutex_Unlock(&osi_event_hook_purge_lock);
    }
    hook->state = OSI_EVENT_HOOK_STATE_ONLINE;
    osi_rwlock_Unlock(&hook->lock);

 done:
    return res;

 invalid_state:
    res = OSI_FAIL;
    osi_rwlock_Unlock(&hook->lock);
    goto done;

 wait_race:
    hook->state_waiters++;
    do {
	osi_mutex_Lock(&osi_event_hook_purge_lock);
	osi_rwlock_Unlock(&hook->lock);
	osi_condvar_Wait(&osi_event_hook_purge_cv,
			 &osi_event_hook_purge_lock);
	osi_mutex_Unlock(&osi_event_hook_purge_lock);
	osi_rwlock_WrLock(&hook->lock);
    } while (hook->state == OSI_EVENT_HOOK_STATE_PURGING);
    hook->state_waiters--;
    if (osi_compiler_expect_false(hook->state != OSI_EVENT_HOOK_STATE_ONLINE)) {
	goto invalid_state;
    }
    goto race_over;
}


/*
 * fire an event 
 *
 * [IN] hook   -- pointer to hook object
 * [IN] event  -- event id
 * [IN] sdata  -- opaque data pointer
 *
 * returns:
 *   OSI_OK on success
 */
osi_result
osi_event_fire(osi_event_hook_t * hook,
	       osi_event_id_t event,
	       void * sdata)
{
    osi_result res, code = OSI_OK;
    osi_event_subscription_t * sub;
    osi_event_record_t * record;
    osi_condvar_t * cv;
    osi_mutex_t * lock;

    osi_rwlock_RdLock(&hook->lock);

    for (osi_list_Scan_Immutable(&hook->watchers,
				 sub,
				 osi_event_subscription_t,
				 watchers)) {
	switch (sub->action.type) {
	case OSI_EVENT_ACTION_NONE:
	    res = OSI_OK;
	    break;
	case OSI_EVENT_ACTION_FUNCTION:
	    res = (*sub->action.data.func)(event,
					   sdata,
					   sub,
					   sub->sdata,
					   hook,
					   hook->sdata);
	    break;
	case OSI_EVENT_ACTION_CV:
	    osi_mutex_Lock(sub->action.data.cv->lock);
	    if (sub->action.data.cv->broadcast) {
		osi_condvar_Broadcast(sub->action.data.cv->cv);
	    } else {
		osi_condvar_Signal(sub->action.data.cv->cv);
	    }
	    osi_mutex_Unlock(sub->action.data.cv->lock);
	    res = OSI_OK;
	    break;
	case OSI_EVENT_ACTION_QUEUE:
	    record = osi_mem_object_cache_alloc_nosleep(osi_event_record_cache);
	    if (osi_compiler_expect_true(record != osi_NULL)) {
		record->event_id = event;
		record->event_sdata = sdata;
		record->sub = sub;
		record->sub_sdata = sub->sdata;
		record->hook = hook;
		record->hook_sdata = hook->sdata;

		lock = sub->action.data.queue->lock;
		osi_mutex_Lock(lock);
		osi_list_Append(sub->action.data.queue->list,
				record,
				osi_event_record_t,
				record_queue);
		cv = sub->action.data.queue->cv;
		if (cv) {
		    osi_condvar_Signal(cv);
		}
		osi_mutex_Unlock(lock);
		res = OSI_OK;
	    } else {
		res = OSI_ERROR_NOMEM;
	    }
	    break;
	    
	default:
	    res = OSI_FAIL;
	}
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
    }

    osi_rwlock_Unlock(&hook->lock);

    return code;
}


/*
 * initialize an event subscription object
 *
 * [IN] sub  -- pointer to subscription object
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_event_subscription_Init(osi_event_subscription_t * sub)
{
    osi_result res = OSI_OK;

    osi_mutex_Init(&sub->lock,
		   osi_impl_mutex_opts());
    sub->hook = osi_NULL;
    sub->sdata = osi_NULL;
    sub->action.type = OSI_EVENT_ACTION_NONE;
    sub->action.data.opaque = osi_NULL;

    return res;
}

/*
 * destroy an event subscription object
 *
 * [IN] sub  -- pointer to subscription object
 *
 * preconditions:
 *   subscription object is not subscribed to an event hook
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when subscribed
 */
osi_result
osi_event_subscription_Destroy(osi_event_subscription_t * sub)
{
    osi_result res = OSI_OK;

    if (osi_compiler_expect_false(sub->hook != osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    osi_mutex_Destroy(&sub->lock);

 error:
    return res;
}

/*
 * subscribe to an event hook
 *
 * [IN] hook  -- pointer to an event hook object
 * [IN] sub   -- pointer to an event subscription object
 *
 * preconditions:
 *   sub object MUST NOT be subscribed to any hook
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_event_subscribe(osi_event_hook_t * hook,
		    osi_event_subscription_t * sub)
{
    osi_result res = OSI_OK;

    osi_mutex_Lock(&sub->lock);
    osi_rwlock_WrLock(&hook->lock);
    sub->hook = hook;
    osi_list_Append(&hook->watchers,
		    sub,
		    osi_event_subscription_t,
		    watchers);
    osi_rwlock_Unlock(&hook->lock);
    osi_mutex_Unlock(&sub->lock);

    return res;
}

/*
 * unsubscribe from an event hook
 *
 * [IN] sub  -- pointer to an event subscription object
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on invalid hook state
 */
osi_result
osi_event_unsubscribe(osi_event_subscription_t * sub)
{
    osi_result res = OSI_OK;
    osi_event_hook_t * hook;

    osi_mutex_Lock(&sub->lock);
    hook = sub->hook;
    if (osi_compiler_expect_false(hook == osi_NULL)) {
	goto error_sync;
    }

    osi_rwlock_WrLock(&hook->lock);
    switch (hook->state) {
    case OSI_EVENT_HOOK_STATE_ONLINE:
	break;
    case OSI_EVENT_HOOK_STATE_PURGING:
	goto wait_race;
    default:
	osi_rwlock_Unlock(&hook->lock);
	res = OSI_FAIL;
	goto error_sync;
    }

 race_over:
    osi_list_Remove(sub,
		    osi_event_subscription_t,
		    watchers);
    sub->hook = osi_NULL;

    if (hook->state_waiters) {
	osi_mutex_Lock(&osi_event_hook_purge_lock);
	osi_condvar_Signal(&osi_event_hook_purge_cv);
	osi_mutex_Unlock(&osi_event_hook_purge_lock);
    }

    osi_rwlock_Unlock(&hook->lock);

 error_sync:
    osi_mutex_Unlock(&sub->lock);
    return res;

 wait_race:
    hook->state_waiters++;
    do {
	osi_mutex_Lock(&osi_event_hook_purge_lock);
	osi_rwlock_Unlock(&hook->lock);
	osi_mutex_Unlock(&sub->lock);
	osi_condvar_Wait(&osi_event_hook_purge_cv,
			 &osi_event_hook_purge_lock);
	osi_mutex_Unlock(&osi_event_hook_purge_lock);
	osi_mutex_Lock(&sub->lock);
	if (sub->hook != hook) {
	    /* XXX this condition poses an interesting
	     * problem for the state_waiters field
	     *
	     * for now, we just leak a ref. the only downside
	     * to this is that unnecessary cond_signals will
	     * occur */
	    res = OSI_FAIL;
	    goto error_sync;
	}
	osi_rwlock_WrLock(&hook->lock);
    } while (hook->state == OSI_EVENT_HOOK_STATE_PURGING);
    hook->state_waiters--;
    if (osi_compiler_expect_false(hook->state != OSI_EVENT_HOOK_STATE_ONLINE)) {
	osi_rwlock_Unlock(&hook->lock);
	res = OSI_FAIL;
	goto error_sync;
    }
    goto race_over;
}

/*
 * clear any previous event actions
 *
 * [IN] sub    -- pointer to an event subscription object
 *
 * preconditions:
 *   subscription object MUST NOT be subscribed to an event hook
 *   sub->lock held
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
__osi_event_action_clear(osi_event_subscription_t * sub)
{
    osi_result res = OSI_OK;

    switch(sub->action.type) {
    case OSI_EVENT_ACTION_NONE:
	goto done;
    case OSI_EVENT_ACTION_CV:
	osi_mem_object_cache_free(osi_event_action_cv_cache,
				  sub->action.data.cv);
	break;
    case OSI_EVENT_ACTION_QUEUE:
	osi_mem_object_cache_free(osi_event_action_queue_cache,
				  sub->action.data.queue);
	break;
    default:
	break;
    }

    sub->action.type = OSI_EVENT_ACTION_NONE;
    sub->action.data.opaque = osi_NULL;

 done:
    return res;
}

/*
 * setup a no-op event action
 *
 * [IN] sub    -- pointer to an event subscription object
 *
 * preconditions:
 *   subscription object MUST NOT be subscribed to an event hook
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on failure
 */
osi_result
osi_event_action_none(osi_event_subscription_t * sub)
{
    osi_result res = OSI_OK;

    osi_mutex_Lock(&sub->lock);
    if (osi_compiler_expect_false(sub->hook != osi_NULL)) {
	res = OSI_FAIL;
	goto error_sync;
    }

    res = __osi_event_action_clear(sub);

 error_sync:
    osi_mutex_Unlock(&sub->lock);
    return res;
}

/*
 * setup an event action function
 *
 * [IN] sub    -- pointer to an event subscription object
 * [IN] fp     -- action function pointer
 * [IN] sdata  -- opaque data pointer
 *
 * preconditions:
 *   subscription object MUST NOT be subscribed to an event hook
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on failure
 */
osi_result
osi_event_action_func(osi_event_subscription_t * sub,
		      osi_event_action_func_t * fp,
		      void * sdata)
{
    osi_result res = OSI_OK;

    osi_mutex_Lock(&sub->lock);
    if (osi_compiler_expect_false(sub->hook != osi_NULL)) {
	res = OSI_FAIL;
	goto error_sync;
    }

    res = __osi_event_action_clear(sub);

    sub->action.type = OSI_EVENT_ACTION_FUNCTION;
    sub->action.data.func = fp;
    sub->sdata = sdata;

 error_sync:
    osi_mutex_Unlock(&sub->lock);
    return res;
}

/*
 * setup an event action condvar
 *
 * [IN] sub        -- pointer to an event subscription object
 * [IN] lock       -- pointer to lock object
 * [IN] cv         -- pointer to condition variable object
 * [IN] broadcast  -- non-zero if cv should be broadcasted
 * [IN] sdata      -- opaque data pointer
 *
 * preconditions:
 *   subscription object MUST NOT be subscribed to an event hook
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on failure
 */
osi_result
osi_event_action_cv(osi_event_subscription_t * sub,
		    osi_mutex_t * lock,
		    osi_condvar_t * cv,
		    osi_uint32 broadcast,
		    void * sdata)
{
    osi_result res = OSI_OK;
    osi_event_action_cv_t * action;

    osi_mutex_Lock(&sub->lock);
    if (osi_compiler_expect_false(sub->hook != osi_NULL)) {
	res = OSI_FAIL;
	goto error_sync;
    }

    res = __osi_event_action_clear(sub);

    action = osi_mem_object_cache_alloc_nosleep(osi_event_action_cv_cache);
    if (osi_compiler_expect_false(action == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error_sync;
    }

    action->lock = lock;
    action->cv = cv;
    action->broadcast = broadcast;

    sub->action.type = OSI_EVENT_ACTION_CV;
    sub->action.data.cv = action;
    sub->sdata = sdata;

 error_sync:
    osi_mutex_Unlock(&sub->lock);
    return res;
}

/*
 * setup an event action queue
 *
 * [IN] sub    -- pointer to an event subscription object
 * [IN] lock   -- pointer to lock object
 * [IN] cv     -- pointer to condition variable object
 * [IN] list   -- pointer to list head object
 * [IN] sdata  -- opaque data pointer
 *
 * preconditions:
 *   subscription object MUST NOT be subscribed to an event hook
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on failure
 */
osi_result
osi_event_action_queue(osi_event_subscription_t * sub,
		       osi_mutex_t * lock,
		       osi_condvar_t * cv,
		       osi_list_head_volatile * list,
		       void * sdata)
{
    osi_result res = OSI_OK;
    osi_event_action_queue_t * action;

    osi_mutex_Lock(&sub->lock);
    if (osi_compiler_expect_false(sub->hook != osi_NULL)) {
	res = OSI_FAIL;
	goto error_sync;
    }

    res = __osi_event_action_clear(sub);

    action = osi_mem_object_cache_alloc_nosleep(osi_event_action_queue_cache);
    if (osi_compiler_expect_false(action == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error_sync;
    }

    action->lock = lock;
    action->cv = cv;
    action->list = list;

    sub->action.type = OSI_EVENT_ACTION_QUEUE;
    sub->action.data.queue = action;
    sub->sdata = sdata;

 error_sync:
    osi_mutex_Unlock(&sub->lock);
    return res;
}


osi_result
osi_event_PkgInit(void)
{
    osi_result res = OSI_OK;

    osi_mutex_Init(&osi_event_hook_purge_lock,
		   osi_impl_mutex_opts());
    osi_condvar_Init(&osi_event_hook_purge_cv,
		     osi_impl_condvar_opts());

    osi_event_record_cache =
	osi_mem_object_cache_create("osi_event_record_cache",
				    sizeof(osi_event_record_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_impl_mem_object_cache_opts());
    if (osi_event_record_cache == osi_NULL) {
	res = OSI_FAIL;
	goto error;
    }

    osi_event_action_cv_cache =
	osi_mem_object_cache_create("osi_event_action_cv_cache",
				    sizeof(osi_event_action_cv_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_impl_mem_object_cache_opts());
    if (osi_event_record_cache == osi_NULL) {
	res = OSI_FAIL;
	goto error;
    }

    osi_event_action_queue_cache =
	osi_mem_object_cache_create("osi_event_action_queue_cache",
				    sizeof(osi_event_action_queue_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_impl_mem_object_cache_opts());
    if (osi_event_record_cache == osi_NULL) {
	res = OSI_FAIL;
	goto error;
    }

 error:
    return res;
}

osi_result
osi_event_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    osi_mutex_Destroy(&osi_event_hook_purge_lock);
    osi_condvar_Destroy(&osi_event_hook_purge_cv);

    osi_mem_object_cache_destroy(osi_event_record_cache);
    osi_mem_object_cache_destroy(osi_event_action_cv_cache);
    osi_mem_object_cache_destroy(osi_event_action_queue_cache);

    return res;
}
