/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi.h>
#include <osi/osi_trace.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_list.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_object_cache.h>
#include <trace/consumer/cursor.h>
#include <trace/consumer/consumer.h>
#include <trace/syscall.h>
#include <trace/agent/console.h>
#include <trace/agent/trap.h>
#include <trace/rpc/console_api.h>
#include <rx/rx.h>
#include <afs/afsutil.h>


/*
 * osi tracing framework
 * trace consumer process
 * console registry
 */

#define OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS 4

struct {
    osi_list_head_volatile console_list;
    osi_rwlock_t lock;
} osi_trace_console_registry;

osi_mem_object_cache_t * osi_trace_console_cache = osi_NULL;
osi_mem_object_cache_t * osi_trace_console_addr_cache[OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS];
osi_uint32 osi_trace_console_addr_cache_index[OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS];
osi_uint32 osi_trace_console_addr_cache_len[OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS];
char * osi_trace_console_addr_cache_name[OSI_TRACE_CONSOLE_ADDR_CACHE_BUCKETS];

osi_static struct rx_securityClass * osi_trace_console_sc = osi_NULL;

osi_static osi_result osi_trace_console_rgy_console_alloc(osi_trace_console_t **);
osi_static osi_result osi_trace_console_rgy_console_free(osi_trace_console_t *);
osi_static osi_result osi_trace_console_rgy_console_lookup(osi_trace_console_handle_t *,
							   osi_trace_console_t **);

osi_static osi_result osi_trace_console_rgy_console_addr_copy(osi_trace_console_addr_t * dst,
							      osi_trace_console_addr_t * src);

osi_static int osi_trace_console_addr_cache_ctor(void * buf, void * sdata, int flags);


/*
 * addr cache entry vector constructor
 *
 * [IN] buf    -- newly allocated buffer
 * [IN] sdata  -- allocator-specific data
 * [IN] flags  -- allocation flags
 *
 * returns:
 *   0 on success
 *   non-zero on failure
 */
osi_static int
osi_trace_console_addr_cache_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_console_addr_t * addr;
    osi_uint32 * idx;
    int i;

    idx = (osi_uint32 *) sdata;
    addr = (osi_trace_console_addr_t *) buf;

    addr->cache_idx = *idx;

    for (i = 0; i < osi_trace_console_addr_cache_len[addr->cache_idx]; i++) {
	addr->conn = osi_NULL;
    }

    return 0;
}

/*
 * allocate an osi_trace_console_t object
 *
 * [INOUT] console_out  -- address in which to store pointer to allocated object
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
 * [IN] handle          -- console handle
 * [INOUT] console_out  -- address in which to store console pointer
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
					 size_t addr_vec_len)
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
			       size_t addr_vec_len)
{
    osi_result res = OSI_OK;
    osi_trace_console_t * console = osi_NULL;
    size_t i;

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
		       size_t trap_payload_len)
{
    return OSI_UNIMPLEMENTED;
}

osi_result
osi_trace_console_rgy_PkgInit(void)
{
    osi_result res = OSI_OK;
    int i;
    osi_rwlock_options_t rw_opts;
    osi_mem_object_cache_options_t cache_opts;

    osi_rwlock_options_Init(&rw_opts);
    osi_rwlock_options_Set(&rw_opts, OSI_RWLOCK_OPTION_TRACE_ALLOWED, 0);
    osi_mem_object_cache_options_Init(&cache_opts);
    osi_mem_object_cache_options_Set(&cache_opts,
				     OSI_MEM_OBJECT_CACHE_OPTION_TRACE_ALLOWED, 
				     0);

    osi_rwlock_Init(&osi_trace_console_registry.lock,
		    &rw_opts);

    /* setup the main console cache */
    osi_trace_console_cache = 
	osi_mem_object_cache_create("osi_trace_console",
				    sizeof(osi_trace_console_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    &cache_opts);
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
					&cache_opts);
	if (osi_compiler_expect_false(osi_trace_console_addr_cache[i] == osi_NULL)) {
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

osi_result
osi_trace_console_rgy_PkgShutdown(void)
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
    osi_mem_object_cache_destroy(osi_trace_console_cache);
    osi_rwlock_Destroy(&osi_trace_console_registry.lock);

    return res;
}
