/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_list.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_string.h>
#include <osi/osi_event.h>
#include <trace/directory.h>
#include <trace/consumer/cursor.h>
#include <trace/consumer/consumer.h>
#include <trace/consumer/cache/generator.h>
#include <trace/syscall.h>
#include <trace/agent/console.h>
#include <trace/agent/console_impl.h>
#include <trace/agent/console_impl_types.h>
#include <trace/agent/trap.h>
#include <trace/rpc/console_api.h>
#include <rx/rx.h>
#include <afs/afsutil.h>


/*
 * osi tracing framework
 * trace consumer process
 * console registry
 */

struct {
    osi_list_head_volatile console_list;
    osi_rwlock_t lock;
} osi_trace_console_registry;

osi_mem_object_cache_t * osi_trace_console_cache = osi_NULL;

#define OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS 4
osi_mem_object_cache_t * osi_trace_console_addr_cache[OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS];
osi_uint32 osi_trace_console_addr_cache_index[OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS];
osi_uint32 osi_trace_console_addr_cache_len[OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS];
char * osi_trace_console_addr_cache_name[OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS];

#define OSI_TRACE_CONSOLE_PROBE_ENA_CACHE_BUCKETS 4
osi_mem_object_cache_t * osi_trace_console_probe_ena_cache[OSI_TRACE_CONSOLE_PROBE_ENA_CACHE_BUCKETS];
osi_uint8 osi_trace_console_probe_ena_cache_index[OSI_TRACE_CONSOLE_PROBE_ENA_CACHE_BUCKETS];
osi_uint32 osi_trace_console_probe_ena_cache_len[OSI_TRACE_CONSOLE_PROBE_ENA_CACHE_BUCKETS];
char * osi_trace_console_probe_ena_cache_name[OSI_TRACE_CONSOLE_PROBE_ENA_CACHE_BUCKETS];

osi_static struct rx_securityClass * osi_trace_console_sc = osi_NULL;

osi_static osi_result osi_trace_console_rgy_console_alloc(osi_trace_console_t **);
osi_static osi_result osi_trace_console_rgy_console_free(osi_trace_console_t *);
osi_static osi_result osi_trace_console_rgy_console_lookup(osi_trace_console_handle_t *,
							   osi_trace_console_t **);

osi_static osi_result osi_trace_console_rgy_console_addr_copy(osi_trace_console_addr_t * dst,
							      osi_trace_console_addr_t * src);

osi_static osi_result
osi_trace_console_rgy_probe_ena_cache_alloc(osi_trace_console_probe_ena_cache_t ** ena_out,
					    osi_size_t filter_len);
osi_static osi_result
osi_trace_console_rgy_probe_ena_cache_free(osi_trace_console_probe_ena_cache_t * ena);

osi_static osi_result
osi_trace_console_rgy_probe_ena_cache_event_handler(osi_event_id_t,
						    void * event_rock,
						    osi_event_subscription_t *,
						    void * sub_rock,
						    osi_event_hook_t *,
						    void * hook_rock);

OSI_MEM_OBJECT_CACHE_CTOR_STATIC_PROTOTYPE(osi_trace_console_addr_cache_ctor);
OSI_MEM_OBJECT_CACHE_CTOR_STATIC_PROTOTYPE(osi_trace_console_probe_ena_cache_ctor);
OSI_MEM_OBJECT_CACHE_DTOR_STATIC_PROTOTYPE(osi_trace_console_probe_ena_cache_dtor);


/*
 * addr cache entry vector constructor
 */
OSI_MEM_OBJECT_CACHE_CTOR_STATIC_DECL(osi_trace_console_addr_cache_ctor)
{
    osi_trace_console_addr_t * addr = OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF;
    osi_uint32 * idx = OSI_MEM_OBJECT_CACHE_FUNC_ARG_ROCK;
    int i;

    addr->cache_idx = *idx;

    for (i = 0; i < osi_trace_console_addr_cache_len[addr->cache_idx]; i++) {
	addr->conn = osi_NULL;
    }

    return 0;
}

/*
 * probe enablement cache entry constructor
 */
OSI_MEM_OBJECT_CACHE_CTOR_STATIC_DECL(osi_trace_console_probe_ena_cache_ctor)
{
    osi_trace_console_probe_ena_cache_t * ena = OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF;
    osi_uint8 * idx = OSI_MEM_OBJECT_CACHE_FUNC_ARG_ROCK;
    int i;

    osi_event_subscription_Init(&ena->gen_sub);
    osi_event_action_func(&ena->gen_sub,
			  &osi_trace_console_rgy_probe_ena_cache_event_handler,
			  ena);
    ena->cache_idx = *idx;

    return 0;
}

/*
 * probe enablement cache entry destructor
 */
OSI_MEM_OBJECT_CACHE_DTOR_STATIC_DECL(osi_trace_console_probe_ena_cache_dtor)
{
    osi_trace_console_probe_ena_cache_t * ena = OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF;

    osi_event_subscription_Destroy(&ena->gen_sub);
}

/*
 * allocate an osi_trace_console_t object
 *
 * [OUT] console_out  -- address in which to store pointer to allocated object
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result 
osi_trace_console_rgy_console_alloc(osi_trace_console_t ** console_out)
{
    osi_result res = OSI_OK;
    osi_trace_console_t * console;

    *console_out = console = (osi_trace_console_t *)
	osi_mem_object_cache_alloc(osi_trace_console_cache);
    if (osi_compiler_expect_false(console == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    console->console_addr_vec = osi_NULL;

 error:
    return res;
}

/*
 * free an osi_trace_console_t object
 *
 * [IN] console  -- pointer to object to free
 *
 * returns:
 *   OSI_OK always
 */
osi_static 
osi_result osi_trace_console_rgy_console_free(osi_trace_console_t * console)
{
    osi_mem_object_cache_free(osi_trace_console_cache, console);
    return OSI_OK;
}

/*
 * lookup an osi_trace_console_t object by handle
 *
 * [IN] handle        -- console handle
 * [OUT] console_out  -- address in which to store console pointer
 *
 * preconditions:
 *   osi_trace_console_registry.lock held either shared or exclusive
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on error
 */
osi_static osi_result 
osi_trace_console_rgy_console_lookup(osi_trace_console_handle_t * handle,
				     osi_trace_console_t ** console_out)
{
    osi_result res = OSI_FAIL;
    osi_trace_console_t * console;

    for (osi_list_Scan_Immutable(&osi_trace_console_registry.console_list,
				 console,
				 osi_trace_console_t, console_list)) {
	if (afs_uuid_equal(handle, &console->console_handle)) {
	    *console_out = console;
	    res = OSI_OK;
	    break;
	}
    }

    return res;
}

/*
 * allocate an osi_trace_console_addr_t vector
 *
 * [INOUT] addr_vec_out  -- address at which to store newly allocated vector pointer
 * [IN] addr_vec_len     -- desired length of vector
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *
 * notes:
 *   we do not check for illegal addr_vec_len value. caller is responsible for this 
 *   verification step.
 */
osi_result
osi_trace_console_rgy_console_addr_alloc(osi_trace_console_addr_t ** addr_vec_out,
					 osi_size_t addr_vec_len)
{
    osi_result res = OSI_OK;
    osi_uint32 cache_idx;
    osi_trace_console_addr_t * addr_vec = osi_NULL;
    int i;

    /* do binary search to find optimal cache from which to allocate */
    if (addr_vec_len <= osi_trace_console_addr_cache_len[1]) {
	if (addr_vec_len <= osi_trace_console_addr_cache_len[0]) {
	    cache_idx = 0;
	} else {
	    cache_idx = 1;
	}
    } else {
	if (addr_vec_len <= osi_trace_console_addr_cache_len[2]) {
	    cache_idx = 2;
	} else {
	    cache_idx = 3;
	}
    }

    *addr_vec_out = addr_vec = (osi_trace_console_addr_t *)
	osi_mem_object_cache_alloc(osi_trace_console_addr_cache[cache_idx]);
    if (osi_compiler_expect_false(addr_vec == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

/*
 * free an osi_trace_console_addr_t vector
 * also free any associated rx_connection objects
 *
 * [IN] addr  -- pointer to address vector
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_console_rgy_console_addr_free(osi_trace_console_addr_t * addr)
{
    int i;

    for (i = 0; i < osi_trace_console_addr_cache_len[addr->cache_idx]; i++) {
	if (addr->conn) {
	    rx_DestroyConnection(addr->conn);
	    addr->conn = osi_NULL;
	}
    }
    osi_mem_object_cache_free(osi_trace_console_addr_cache[addr->cache_idx],
			      addr);
    return OSI_OK;
}

/*
 * copy an osi_trace_addr_t
 *
 * [INOUT] dst  -- copy destination
 * [IN] src     -- copy source
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on unknown transport id
 */
osi_static osi_result
osi_trace_console_rgy_console_addr_copy(osi_trace_console_addr_t * dst,
					osi_trace_console_addr_t * src)
{
    osi_result res = OSI_OK;

    dst->transport = src->transport;
    switch(src->transport) {
    case OSI_TRACE_CONSOLE_TRANSPORT_RX_UDP:
    case OSI_TRACE_CONSOLE_TRANSPORT_RX_TCP:
	dst->conn = rx_NewConnection(src->addr.addr4.addr,
				     src->addr.addr4.port,
				     OSI_TRACE_RPC_SERVICE_CONSOLE,
				     osi_trace_console_sc,
				     0);
	rx_SetConnDeadTime(dst->conn, 50);
	rx_SetConnHardDeadTime(dst->conn, OSI_TRACE_RPC_HARDDEADTIME);

    case OSI_TRACE_CONSOLE_TRANSPORT_SNMP_V1:
	dst->addr.addr4.addr = src->addr.addr4.addr;
	dst->addr.addr4.port = src->addr.addr4.port;
	dst->addr.addr4.flags = src->addr.addr4.flags;
	break;

    default:
	res = OSI_FAIL;
    }

    return res;
}


/*
 * allocate an osi_trace_console_probe_ena_cache_t vector
 *
 * [OUT] ena_out    -- address at which to store newly allocated cache object pointer
 * [IN] filter_len  -- desired length of vector
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *
 * notes:
 *   we do not check for illegal filter_len value. caller is responsible for this 
 *   verification step.
 */
osi_static osi_result
osi_trace_console_rgy_probe_ena_cache_alloc(osi_trace_console_probe_ena_cache_t ** ena_out,
					    osi_size_t filter_len)
{
    osi_result res = OSI_OK;
    osi_uint32 cache_idx;
    osi_trace_console_probe_ena_cache_t * ena = osi_NULL;
    int i;

    /* do binary search to find optimal cache from which to allocate */
    if (filter_len <= osi_trace_console_probe_ena_cache_len[1]) {
	if (filter_len <= osi_trace_console_probe_ena_cache_len[0]) {
	    cache_idx = 0;
	} else {
	    cache_idx = 1;
	}
    } else {
	if (filter_len <= osi_trace_console_probe_ena_cache_len[2]) {
	    cache_idx = 2;
	} else {
	    cache_idx = 3;
	}
    }

    *ena_out = ena = (osi_trace_console_probe_ena_cache_t *)
	osi_mem_object_cache_alloc(osi_trace_console_probe_ena_cache[cache_idx]);
    if (osi_compiler_expect_false(ena == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

/*
 * free an osi_trace_console_probe_ena_cache_t object
 *
 * [IN] ena  -- pointer to probe_ena_cache object
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_trace_console_rgy_probe_ena_cache_free(osi_trace_console_probe_ena_cache_t * ena)
{
    osi_mem_object_cache_free(osi_trace_console_probe_ena_cache[ena->cache_idx],
			      ena);
    return OSI_OK;
}

/*
 * lookup an ena record
 *
 * [IN] handle    -- pointer to console handle
 * [IN] gen_id    -- generator id receiving filter
 * [IN] filter    -- probe filter string
 * [OUT] ena_out  -- address in which to store ena object pointer
 *
 * preconditions:
 *   osi_trace_console_registry.lock held either shared or exclusive
 *
 * returns:
 *   OSI_OK on success
 *   see osi_trace_console_rgy_console_lookup()
 */
osi_result
osi_trace_console_rgy_probe_ena_lookup(osi_trace_console_t * console,
				       osi_trace_gen_id_t gen_id,
				       const char * filter,
				       osi_trace_console_probe_ena_cache_t ** ena_out)
{
    osi_result code = OSI_FAIL;
    osi_trace_console_probe_ena_cache_t * ena;

    *ena_out = osi_NULL;
    for (osi_list_Scan_Immutable(&console->probe_ena_list,
				 ena,
				 osi_trace_console_probe_ena_cache_t,
				 ena_list)) {
	if ((ena->gen_id == gen_id) && 
	    !osi_string_cmp(filter, ena->filter)) {
	    *ena_out = ena;
	    code = OSI_OK;
	    break;
	}
    }

    return code;
}

/*
 * handle inbound events from osi_trace_consumer_gen_cache_t objects
 *
 * [IN] event       -- event id
 * [IN] event_rock  -- rock containing pointer to osi_trace_consumer_probe_val_cache_t object
 * [IN] sub         -- subscription object
 * [IN] sub_rock    -- rock containing pointer to osi_trace_console_t object
 * [IN] hook        -- hook object
 * [IN] hook_rock   -- rock containing pointer to osi_trace_consumer_gen_cache_t object
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_trace_console_rgy_probe_ena_cache_event_handler(osi_event_id_t event,
						    void * event_rock,
						    osi_event_subscription_t * sub,
						    void * sub_rock,
						    osi_event_hook_t * hook,
						    void * hook_rock)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_gen_cache_t * gen = hook_rock;
    osi_trace_consumer_probe_val_cache_t * probe_value = event_rock;
    osi_trace_console_t * cons = sub_rock;
    osi_trace_console_probe_ena_cache_t * ena;

    switch(event) {
    case OSI_TRACE_CONSUMER_GEN_CACHE_EVENT_PROBE_REGISTER:
	osi_rwlock_WrLock(&osi_trace_console_registry.lock);
	for (osi_list_Scan_Immutable(&cons->probe_ena_list,
				     ena,
				     osi_trace_console_probe_ena_cache_t,
				     ena_list)) {
	    /*
	     * XXX compare new probe name to filter expression in ena object
	     */
	    if (1/*XXX*/) {
		/* XXX we have a match ; do an enable call */
	    }
	}
	osi_rwlock_Unlock(&osi_trace_console_registry.lock);
	break;
    }

    return res;
}

/*
 * register a trace console
 *
 * [IN] handle
 * [IN] addr_vec
 * [IN] addr_vec_len
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_console_rgy_register(osi_trace_console_handle_t * handle,
			       osi_trace_console_addr_t * addr_vec,
			       osi_size_t addr_vec_len)
{
    osi_result res = OSI_OK;
    osi_trace_console_t * console = osi_NULL;
    osi_size_t i;

    osi_rwlock_WrLock(&osi_trace_console_registry.lock);
    res = osi_trace_console_rgy_console_lookup(handle, &console);
    if (OSI_RESULT_OK(res)) {
	/* a previous registration exists; replace the mh addr vec */
	if (console->console_addr_vec) {
	    osi_trace_console_rgy_console_addr_free(console->console_addr_vec);
	    console->console_addr_vec = osi_NULL;
	}
    } else {
	/* if no console registration exists, create a new one */
	res = osi_trace_console_rgy_console_alloc(&console);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}
    }

    /* allocate a multihome address vector */
    res = osi_trace_console_rgy_console_addr_alloc(&console->console_addr_vec,
						   addr_vec_len);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    /* populate the mh addr vector */
    for (i = 0; i < addr_vec_len; i++) {
	res = osi_trace_console_rgy_console_addr_copy(&console->console_addr_vec[i],
						      &addr_vec[i]);
    }

    /* register the console structure */
    osi_list_Append(&osi_trace_console_registry.console_list,
		    console, osi_trace_console_t, console_list);

    osi_rwlock_Unlock(&osi_trace_console_registry.lock);

 done:
    return res;

 error:
    if (console) {
	if (console->console_addr_vec) {
	    osi_trace_console_rgy_console_addr_free(console->console_addr_vec);
	    console->console_addr_vec = osi_NULL;
	}
	osi_trace_console_rgy_console_free(console);
    }
    goto done;
}

/*
 * unregister a trace console
 *
 * [IN] handle
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on unknown console
 */
osi_result
osi_trace_console_rgy_unregister(osi_trace_console_handle_t * handle)
{
    osi_result res = OSI_FAIL;
    osi_trace_console_t * console;

    osi_rwlock_WrLock(&osi_trace_console_registry.lock);
    res = osi_trace_console_rgy_console_lookup(handle, &console);
    if (OSI_RESULT_OK_LIKELY(res)) {
	if (console->console_addr_vec) {
	    osi_trace_console_rgy_console_addr_free(console->console_addr_vec);
	    console->console_addr_vec = osi_NULL;
	}
	osi_list_Remove(console, osi_trace_console_t, console_list);
	osi_trace_console_rgy_console_free(console);
	res = OSI_OK;
    }
    osi_rwlock_Unlock(&osi_trace_console_registry.lock);

    return res;
}

/*
 * enable traps to a console
 *
 * [IN] console_handle  -- uuid of console
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on uuid lookup failure
 */
osi_result
osi_trace_console_rgy_enable(osi_trace_console_handle_t * console_handle)
{
    osi_result res;
    osi_trace_console_t * console;

    osi_rwlock_WrLock(&osi_trace_console_registry.lock);
    res = osi_trace_console_rgy_console_lookup(console_handle, &console);
    if (OSI_RESULT_OK_LIKELY(res)) {
	console->flags |= OSI_TRACE_CONSOLE_FLAG_TRAP_ENA;
    }
    osi_rwlock_Unlock(&osi_trace_console_registry.lock);

    return res;
}

/*
 * temporarily disable traps to a console
 *
 * [IN] console_handle  -- uuid of console
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on uuid lookup failure
 */
osi_result
osi_trace_console_rgy_disable(osi_trace_console_handle_t * console_handle)
{
    osi_result res;
    osi_trace_console_t * console;

    osi_rwlock_WrLock(&osi_trace_console_registry.lock);
    res = osi_trace_console_rgy_console_lookup(console_handle, &console);
    if (OSI_RESULT_OK_LIKELY(res)) {
	console->flags &= ~(OSI_TRACE_CONSOLE_FLAG_TRAP_ENA);
    }
    osi_rwlock_Unlock(&osi_trace_console_registry.lock);

    return res;
}

/*
 * perform probe enable on behalf of a console
 *
 * [IN] handle  -- pointer to console handle
 * [IN] gen_id  -- generator id receiving filter
 * [IN] filter  -- probe filter string
 * [OUT] nhits  -- address in which to store filter hit count
 *
 * returns:
 *   OSI_OK on success
 *   see osi_trace_console_rgy_console_lookup()
 *   see osi_trace_console_rgy_probe_ena_cache_alloc()
 */
osi_result
osi_trace_console_rgy_probe_enable(osi_trace_console_handle_t * handle,
				   osi_trace_gen_id_t gen_id,
				   const char * filter,
				   osi_uint32 * nhits)
{
    osi_result res, code = OSI_OK;
    osi_trace_console_t * console, * console2;
    osi_trace_console_probe_ena_cache_t * ena;
    osi_size_t filter_len;
    osi_trace_consumer_gen_cache_t * gen = osi_NULL;

    /*
     * determine filter length, and allocate
     * an appropriately sized record from the cache
     */
    filter_len = osi_string_len(filter);

    if (filter_len >= OSI_TRACE_MAX_PROBE_NAME_LEN) {
	code = OSI_TRACE_DIRECTORY_ERROR_PROBE_FILTER_INVALID;
	goto error;
    }

    code = osi_trace_console_rgy_probe_ena_cache_alloc(&ena,
						       filter_len);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }
    ena->gen_id = gen_id;
    osi_string_cpy(ena->filter, filter);
    ena->state = OSI_TRACE_CONSOLE_RGY_PROBE_STATE_ENABLING;

    /*
     * get a ref to the consumer gen cache object
     */
    code = osi_trace_consumer_gen_cache_lookup(gen_id,
					       &gen);
    if (OSI_RESULT_FAIL(code)) {
	goto cleanup;
    }

    /*
     * pivot the new probe enablement record into the registry
     */
    osi_rwlock_WrLock(&osi_trace_console_registry.lock);
    code = osi_trace_console_rgy_console_lookup(handle,
						&console);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto cleanup_sync;
    }

    osi_list_Append(&console->probe_ena_list,
		    ena,
		    osi_trace_console_probe_ena_cache_t,
		    ena_list);
    osi_rwlock_Unlock(&osi_trace_console_registry.lock);


    /* send a probe enable request message, and wait for a response */
    res = osi_TracePoint_EnableByFilter(gen_id,
					filter,
					nhits);


    /* now try to transition ena record to appropriate state */
    osi_rwlock_WrLock(&osi_trace_console_registry.lock);
    /* 
     * XXX this is an ugly hack
     * someday we need to transition console structures over to using
     * reference counting, and some form of state enumeration.
     * until then, this will have to suffice...
     */
    code = osi_trace_console_rgy_console_lookup(handle,
						&console2);
    if (OSI_RESULT_OK_LIKELY(code)) {
	if (console2 == console) {
	    /* ok, console registry state hasn't changed out from under us... */
	    if (OSI_RESULT_OK_LIKELY(res)) {
		/* probe enable call succeeded */
		ena->state = OSI_TRACE_CONSOLE_RGY_PROBE_STATE_ENABLED;
		res = osi_trace_consumer_gen_cache_event_subscribe(gen,
								   console2);
		if (OSI_RESULT_FAIL(res)) {
		    (osi_Msg "%s: failed to subscribe to gen cache probe register event; "
		     "probe enablement cache coherence compromised\n",
		     __osi_func__);
		}
	    } else {
		/* probe enable call failed */
		code = res;
		osi_list_Remove(ena,
				osi_trace_console_probe_ena_cache_t,
				ena_list);
		osi_trace_console_rgy_probe_ena_cache_free(ena);
	    }
	} else {
	    code = OSI_FAIL;
	}
    }
    osi_rwlock_Unlock(&osi_trace_console_registry.lock);

 error:
    if (gen) {
	/*
	 * we allow the gen cache object reference to be returned even if we managed
	 * to subscribe to the event feed.  the reason is we don't want to hold up
	 * id's in the gen_id namespace just because we have outstanding enablement
	 * contexts.  for now, it's perfectly ok to let those ena contexts die because
	 * _all_ ena contexts are keyed off gen_id, not some higher-level key
	 * (e.g. a bin id or programtype)
	 */
	osi_trace_consumer_gen_cache_put(gen);
    }
    return code;

 cleanup_sync:
    osi_rwlock_Unlock(&osi_trace_console_registry.lock);
 cleanup:
    osi_trace_console_rgy_probe_ena_cache_free(ena);
    goto error;
}

/*
 * perform probe disable on behalf of a console
 *
 * [IN] handle  -- pointer to console handle
 * [IN] gen_id  -- generator id receiving filter
 * [IN] filter  -- probe filter string
 * [OUT] nhits  -- address in which to store filter hit count
 *
 * returns:
 *   OSI_OK on success
 *   see osi_trace_console_rgy_console_lookup()
 */
osi_result
osi_trace_console_rgy_probe_disable(osi_trace_console_handle_t * handle,
				    osi_trace_gen_id_t gen_id,
				    const char * filter,
				    osi_uint32 * nhits)
{
    osi_result code;
    osi_trace_console_t * console, * console2;
    osi_trace_console_probe_ena_cache_t * ena, * ena2;

    /*
     * XXX so, be warned that an (unlikely, but still possible) 
     * race which leads to a probe enablement ref leak
     * namely, where we dec refs on a probe object that
     * was registered during our call to disable by filter.
     * the net result is the refcount rolls over to all f's
     */

    /*
     * lookup the ena object, and transition state to disabling
     */
    osi_rwlock_WrLock(&osi_trace_console_registry.lock);
    code = osi_trace_console_rgy_console_lookup(handle,
					       &console);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error_sync;
    }

    code = osi_trace_console_rgy_probe_ena_lookup(console,
						  gen_id,
						  filter,
						  &ena);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error_sync;
    }

    if (ena->state != OSI_TRACE_CONSOLE_RGY_PROBE_STATE_ENABLED) {
	code = OSI_FAIL;
	goto error_sync;
    }
    ena->state = OSI_TRACE_CONSOLE_RGY_PROBE_STATE_DISABLING;
    osi_rwlock_Unlock(&osi_trace_console_registry.lock);


    /*
     * disable the probes
     */
    code = osi_TracePoint_DisableByFilter(gen_id,
					  filter,
					  nhits);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }


    /*
     * lookup the ena object, protect against races, and
     * destroy the ena object
     */
    osi_rwlock_WrLock(&osi_trace_console_registry.lock);
    /* 
     * XXX this is an ugly hack
     * someday we need to transition console structures over to using
     * reference counting, and some form of state enumeration.
     * until then, this will have to suffice...
     */
    code = osi_trace_console_rgy_console_lookup(handle,
					       &console2);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error_sync;
    }
    if (console != console2) {
	/* race */
	code = OSI_FAIL;
	goto error_sync;
    }

    code = osi_trace_console_rgy_probe_ena_lookup(console,
						  gen_id,
						  filter,
						  &ena2);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error_sync;
    }
    if (ena != ena2) {
	/* race */
	code = OSI_FAIL;
	goto error_sync;
    }

    osi_list_Remove(ena2,
		    osi_trace_console_probe_ena_cache_t,
		    ena_list);
    osi_rwlock_Unlock(&osi_trace_console_registry.lock);
    /* XXX is it safe to do this after dropping synch? */
    osi_event_unsubscribe(&ena2->gen_sub);
    osi_trace_console_rgy_probe_ena_cache_free(ena2);

 error:
    return code;

 error_sync:
    osi_rwlock_Unlock(&osi_trace_console_registry.lock);
    goto error;
}

/*
 * conditionally fire a trace trap
 *
 * [IN] gen_id            -- source gen id of the trace record
 * [IN] probe_id          -- probe id the trace record
 * [IN] trap_encoding     -- trap payload encoding
 * [IN] trap_payload      -- pointer to trap payload
 * [IN] trap_payload_len  -- size of trap payload
 *
 */
osi_result
osi_trace_console_trap(osi_trace_gen_id_t gen_id,
		       osi_trace_probe_id_t probe_id,
		       osi_trace_trap_encoding_t trap_encoding,
		       void * trap_payload,
		       osi_size_t trap_payload_len)
{
    return OSI_UNIMPLEMENTED;
}

OSI_INIT_FUNC_DECL(osi_trace_console_rgy_PkgInit)
{
    osi_result res = OSI_OK;
    int i;

    osi_rwlock_Init(&osi_trace_console_registry.lock,
		    osi_trace_impl_rwlock_opts());

    /* setup the main console cache */
    osi_trace_console_cache = 
	osi_mem_object_cache_create("osi_trace_console",
				    sizeof(osi_trace_console_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_trace_console_cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    /* setup the mh addr vector caches */
    osi_trace_console_addr_cache_len[0] = 1;
    osi_trace_console_addr_cache_len[1] = 4;
    osi_trace_console_addr_cache_len[2] = 8;
    osi_trace_console_addr_cache_len[3] = OSI_TRACE_CONSOLE_MH_MAX;

    osi_trace_console_addr_cache_name[0] = "osi_trace_console_addr_1";
    osi_trace_console_addr_cache_name[1] = "osi_trace_console_addr_4";
    osi_trace_console_addr_cache_name[2] = "osi_trace_console_addr_8";
    osi_trace_console_addr_cache_name[3] = 
	"osi_trace_console_addr_" osi_Macro_ToString(OSI_TRACE_CONSOLE_MH_MAX);

    for (i = 0; i < OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS; i++) {
	osi_trace_console_addr_cache_index[i] = i;
	osi_trace_console_addr_cache[i] =
	    osi_mem_object_cache_create(osi_trace_console_addr_cache_name[i],
					osi_trace_console_addr_cache_len[i] *
					sizeof(osi_trace_console_addr_t),
					0,
					&osi_trace_console_addr_cache_index[i],
					&osi_trace_console_addr_cache_ctor,
					osi_NULL,
					osi_NULL,
					osi_trace_impl_mem_object_cache_opts());
	if (osi_compiler_expect_false(osi_trace_console_addr_cache[i] == osi_NULL)) {
	    res = OSI_ERROR_NOMEM;
	    goto error;
	}
    }

    /* setup the mh addr vector caches */
    osi_trace_console_probe_ena_cache_len[0] = 16;
    osi_trace_console_probe_ena_cache_len[1] = 32;
    osi_trace_console_probe_ena_cache_len[2] = 64;
    osi_trace_console_probe_ena_cache_len[3] = OSI_TRACE_MAX_PROBE_NAME_LEN;

    osi_trace_console_probe_ena_cache_name[0] = "osi_trace_console_probe_ena_16";
    osi_trace_console_probe_ena_cache_name[1] = "osi_trace_console_probe_ena_32";
    osi_trace_console_probe_ena_cache_name[2] = "osi_trace_console_probe_ena_64";
    osi_trace_console_probe_ena_cache_name[3] = 
	"osi_trace_console_probe_ena_" osi_Macro_ToString(OSI_TRACE_MAX_PROBE_NAME_LEN);

    for (i = 0; i < OSI_TRACE_CONSOLE_PROBE_ENA_CACHE_BUCKETS; i++) {
	osi_trace_console_probe_ena_cache_index[i] = i;
	osi_trace_console_probe_ena_cache[i] =
	    osi_mem_object_cache_create(osi_trace_console_probe_ena_cache_name[i],
					osi_trace_console_probe_ena_cache_len[i] *
					sizeof(osi_trace_console_addr_t),
					0,
					&osi_trace_console_probe_ena_cache_index[i],
					&osi_trace_console_probe_ena_cache_ctor,
					&osi_trace_console_probe_ena_cache_dtor,
					osi_NULL,
					osi_trace_impl_mem_object_cache_opts());
	if (osi_compiler_expect_false(osi_trace_console_probe_ena_cache[i] == osi_NULL)) {
	    res = OSI_ERROR_NOMEM;
	    goto error;
	}
    }

    osi_trace_console_sc = rxnull_NewClientSecurityObject();
    if (osi_compiler_expect_false(osi_trace_console_sc == osi_NULL)) {
	res = OSI_FAIL;
    }

 error:
    osi_rwlock_options_Destroy(&rw_opts);
    osi_mem_object_cache_options_Destroy(&cache_opts);
    return res;
}

OSI_FINI_FUNC_DECL(osi_trace_console_rgy_PkgShutdown)
{
    int i;
    osi_result res = OSI_OK;
    osi_trace_console_t * cons, * ncons;

    osi_rwlock_WrLock(&osi_trace_console_registry.lock);
    for (osi_list_Scan(&osi_trace_console_registry.console_list,
		       cons, ncons, osi_trace_console_t, console_list)) {
	if (cons->console_addr_vec) {
	    osi_trace_console_rgy_console_addr_free(cons->console_addr_vec);
	    cons->console_addr_vec = osi_NULL;
	}
	osi_list_Remove(cons, osi_trace_console_t, console_list);
	osi_trace_console_rgy_console_free(cons);
    }
    osi_rwlock_Unlock(&osi_trace_console_registry.lock);

    for (i = 0; i < OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS; i++) {
	osi_mem_object_cache_destroy(osi_trace_console_addr_cache[i]);
    }
    for (i = 0; i < OSI_TRACE_CONSOLE_PROBE_ENA_CACHE_BUCKETS; i++) {
	osi_mem_object_cache_destroy(osi_trace_console_probe_ena_cache[i]);
    }
    osi_mem_object_cache_destroy(osi_trace_console_cache);
    osi_rwlock_Destroy(&osi_trace_console_registry.lock);

    return res;
}
