/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_EVENT_H
#define _OSI_COMMON_EVENT_H 1

/*
 * osi event action framework
 */

#include <osi/COMMON/event_types.h>

/* someday we will develop a lock-free implementation using
 * atomic operations.  for now, we only provide a lock-driven
 * back-end. */

#include <osi/COMMON/event_lock.h>


osi_extern osi_result osi_event_hook_Init(osi_event_hook_t *);
osi_extern osi_result osi_event_hook_Destroy(osi_event_hook_t *);
osi_extern osi_result osi_event_hook_set_rock(osi_event_hook_t *,
					      void * rock);
osi_extern osi_result osi_event_hook_clear(osi_event_hook_t *);

osi_extern osi_result osi_event_fire(osi_event_hook_t *,
				     osi_event_id_t,
				     void * sdata);

osi_extern osi_result osi_event_subscription_Init(osi_event_subscription_t *);
osi_extern osi_result osi_event_subscription_Destroy(osi_event_subscription_t *);
osi_extern osi_result osi_event_subscribe(osi_event_hook_t *,
					  osi_event_subscription_t *);
osi_extern osi_result osi_event_unsubscribe(osi_event_subscription_t *);

osi_extern osi_result osi_event_action_none(osi_event_subscription_t *);
osi_extern osi_result osi_event_action_func(osi_event_subscription_t *,
					    osi_event_action_func_t *,
					    void * sdata);
osi_extern osi_result osi_event_action_cv(osi_event_subscription_t *,
					  osi_mutex_t *,
					  osi_condvar_t *,
					  osi_uint32 broadcast,
					  void * sdata);
osi_extern osi_result osi_event_action_queue(osi_event_subscription_t *,
					     osi_mutex_t *,
					     osi_condvar_t *,
					     osi_list_head_volatile *,
					     void * sdata);

#endif /* _OSI_COMMON_EVENT_H */
