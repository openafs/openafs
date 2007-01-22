/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_EVENT_H
#define _OSI_OSI_EVENT_H 1


/*
 * platform-independent osi_event API
 *
 *  osi_event_provider_t source;
 *    -- event provider descriptor
 *
 *  osi_event_hook_t hook;
 *    -- event source which can be embedded in any object
 *
 *  osi_event_subscription_t sink;
 *    -- event subscription object
 *
 *  osi_event_action_func_t * fp;
 *    -- event notification function pointer
 *
 *
 * event hook control API:
 *
 *  osi_result osi_event_hook_Init(osi_event_hook_t *);
 *    -- initialize an event hook object
 *
 *  osi_result osi_event_hook_Destroy(osi_event_hook_t *);
 *    -- destroy an event hook object
 *
 *  osi_result osi_event_hook_set_rock(osi_event_hook_t *,
 *                                     void * sdata);
 *    -- set opaque data pointer in hook
 *
 *  osi_result osi_event_hook_clear(osi_event_hook_t *);
 *    -- clear out all subscriptions
 *
 *  osi_result osi_event_fire(osi_event_hook_t *,
 *                            osi_event_id_t,
 *                            void * sdata);
 *    -- fire an event
 *
 *
 * event subscription control API:
 *
 *  osi_result osi_event_subscription_Init(osi_event_subscription_t *);
 *    -- initialize an event subscription object
 *
 *  osi_result osi_event_subscription_Destroy(osi_event_subscription_t *);
 *    -- destroy an event subscription object
 *
 *  osi_result osi_event_subscribe(osi_event_hook_t *, 
 *                                 osi_event_subscription_t *);
 *    -- subscribe to an event hook
 *
 *  osi_result osi_event_unsubscribe(osi_event_subscription_t *);
 *    -- unsubscribe from an event
 *
 *  osi_result osi_event_action_none(osi_event_subscription_t *);
 *    -- setup this subscription to do nothing on event fire
 *
 *  osi_result osi_event_action_func(osi_event_subscription_t *,
 *                                   osi_event_action_func_t *,
 *                                   void * sdata);
 *    -- setup this subscription to call an action function
 *
 *  osi_result osi_event_action_cv(osi_event_subscription_t *,
 *                                 osi_mutex_t *,
 *                                 osi_condvar_t *,
 *                                 osi_uint32 broadcast,
 *                                 void * sdata);
 *    -- setup this subscription to signal or broadcast a condvar
 *
 *  osi_result osi_event_action_queue(osi_event_subscription_t *,
 *                                    osi_mutex_t *,
 *                                    osi_condvar_t *,
 *                                    osi_list_head_volatile *);
 *    -- setup this subscription to dump osi_event_record_t's into a queue
 *
 */


#include <osi/COMMON/event.h>

#endif /* _OSI_OSI_EVENT_H */
