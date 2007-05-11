/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_thread.h>
#include <osi/COMMON/thread_event_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_object_cache.h>

/*
 * common thread event handler support
 */

osi_mem_object_cache_t * osi_thread_run_arg_cache = osi_NULL;
osi_mem_object_cache_t * osi_thread_event_cache = osi_NULL;

struct osi_thread_event_table osi_thread_event_table;

osi_static osi_result osi_thread_event_fire(osi_thread_event_type_t event);

OSI_INIT_FUNC_DECL(osi_thread_event_PkgInit)
{
    osi_result res = OSI_OK;

    osi_list_Init(&osi_thread_event_table.create_events);
    osi_list_Init(&osi_thread_event_table.destroy_events);
    osi_rwlock_Init(&osi_thread_event_table.lock, 
		    osi_impl_rwlock_opts());

    osi_thread_run_arg_cache = 
	osi_mem_object_cache_create("osi_thread_run_arg",
				    sizeof(struct osi_thread_run_arg),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_impl_mem_object_cache_opts());

    osi_thread_event_cache = 
	osi_mem_object_cache_create("osi_thread_event",
				    sizeof(osi_thread_event_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_impl_mem_object_cache_opts());

    return res;
}

OSI_FINI_FUNC_DECL(osi_thread_event_PkgShutdown)
{
    osi_mem_object_cache_destroy(osi_thread_event_cache);
    osi_mem_object_cache_destroy(osi_thread_run_arg_cache);
    osi_rwlock_Destroy(&osi_thread_event_table.lock);
    return OSI_OK;
}

void *
osi_thread_run(void * args_in)
{
    struct osi_thread_run_arg * args;
    osi_result res;
    void * ret;

    args = (struct osi_thread_run_arg *) args_in;
    res = osi_thread_event_fire(OSI_THREAD_EVENT_CREATE);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	ret = osi_NULL;
	goto error;
    }

    ret = (*args->fp)(args->arg);

    (void)osi_thread_event_fire(OSI_THREAD_EVENT_DESTROY);

    osi_thread_options_Destroy(&args->opt);
    __osi_thread_run_args_free(args);

 error:
    return ret;
}

osi_result
osi_thread_event_create(osi_thread_event_t ** ev_out, 
			osi_thread_event_handler_t * fp,
			void * sdata)
{
    osi_result res = OSI_OK;
    osi_thread_event_t * ev;

    ev = (osi_thread_event_t *)
	osi_mem_object_cache_alloc(osi_thread_event_cache);
    if (osi_compiler_expect_false(ev == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    ev->fp = fp;
    ev->sdata = sdata;
    ev->events_mask = 0;

    *ev_out = ev;

 error:
    return res;
}

osi_result
osi_thread_event_destroy(osi_thread_event_t * ev)
{
    osi_result res = OSI_OK;

    if (osi_compiler_expect_false(ev->events_mask)) {
	res = OSI_FAIL;
	goto error;
    }

    ev->fp = osi_NULL;
    ev->sdata = osi_NULL;
    osi_mem_object_cache_free(osi_thread_event_cache,
			      ev);

 error:
    return res;
}

/*
 * attempt to register a thread event handle on one or more event processing queues
 *
 * [IN] ev    -- event handle
 * [IN] mask  -- mask of thread events to which handle is to be registered
 *
 * returns:
 *   OSI_OK if handle is now bound to the events in the mask
 *   OSI_FAIL if unknown events are in the mask
 */
osi_result
osi_thread_event_register(osi_thread_event_t * ev, osi_uint32 mask)
{
    osi_result res = OSI_OK;

    if (mask & ~(OSI_THREAD_EVENT_ALL)) {
	res = OSI_FAIL;
    }

    osi_rwlock_WrLock(&osi_thread_event_table.lock);
    if ((mask & OSI_THREAD_EVENT_CREATE) &&
	!(ev->events_mask & OSI_THREAD_EVENT_CREATE)) {
	osi_list_Append(&osi_thread_event_table.create_events,
			ev, osi_thread_event_t, create_event_list);
    }
    if ((mask & OSI_THREAD_EVENT_DESTROY) &&
	!(ev->events_mask & OSI_THREAD_EVENT_DESTROY)) {
	osi_list_Append(&osi_thread_event_table.destroy_events,
			ev, osi_thread_event_t, destroy_event_list);
    }
    ev->events_mask |= mask;
    osi_rwlock_Unlock(&osi_thread_event_table.lock);

    return res;
}

/*
 * attempt to unregister a thread event handle from one or more event processing queues
 *
 * [IN] ev    -- event handle
 * [IN] mask  -- mask of thread events from which handle is to be unregistered
 *
 * returns:
 *   OSI_OK if handle is no longer bound to any event in the mask
 */
osi_result
osi_thread_event_unregister(osi_thread_event_t * ev, osi_uint32 mask)
{
    osi_result res = OSI_OK;

    if (mask & ~(OSI_THREAD_EVENT_ALL)) {
	res = OSI_FAIL;
    }

    osi_rwlock_WrLock(&osi_thread_event_table.lock);
    if ((mask & OSI_THREAD_EVENT_CREATE) &&
	(ev->events_mask & OSI_THREAD_EVENT_CREATE)) {
	osi_list_Remove(ev, osi_thread_event_t, create_event_list);
    }
    if ((mask & OSI_THREAD_EVENT_DESTROY) &&
	(ev->events_mask & OSI_THREAD_EVENT_DESTROY)) {
	osi_list_Remove(ev, osi_thread_event_t, destroy_event_list);
    }
    ev->events_mask &= ~mask;
    osi_rwlock_Unlock(&osi_thread_event_table.lock);

    return res;
}

osi_static osi_result
osi_thread_event_fire(osi_thread_event_type_t event)
{
    osi_result res = OSI_OK, lres;
    osi_thread_event_t * ev;
    osi_thread_id_t id;

    id = osi_thread_current_id();

    osi_rwlock_RdLock(&osi_thread_event_table.lock);
    switch(event) {
    case OSI_THREAD_EVENT_CREATE:
	for (osi_list_Scan_Immutable(&osi_thread_event_table.create_events,
				     ev, osi_thread_event_t, create_event_list)) {
	    lres = (*ev->fp)(id, event, ev->sdata);
	    if (OSI_RESULT_FAIL_UNLIKELY(lres)) {
		res = OSI_FAIL;
	    }
	}
	break;
    case OSI_THREAD_EVENT_DESTROY:
	for (osi_list_Scan_Immutable(&osi_thread_event_table.destroy_events,
				     ev, osi_thread_event_t, destroy_event_list)) {
	    lres = (*ev->fp)(id, event, ev->sdata);
	    if (OSI_RESULT_FAIL_UNLIKELY(lres)) {
		res = OSI_FAIL;
	    }
	}
	break;
    }
    osi_rwlock_Unlock(&osi_thread_event_table.lock);

    return res;	
}

osi_result
__osi_thread_run_args_alloc(struct osi_thread_run_arg ** arg_out)
{
    osi_result res = OSI_OK;
    struct osi_thread_run_arg * arg;

    *arg_out = arg = (struct osi_thread_run_arg *)
	osi_mem_object_cache_alloc(osi_thread_run_arg_cache);
    if (osi_compiler_expect_false(arg == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
    }

    return res;
}

osi_result
__osi_thread_run_args_free(struct osi_thread_run_arg * arg)
{
    osi_mem_object_cache_free(osi_thread_run_arg_cache,
			      arg);
    return OSI_OK;
}
