/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


/*
 * osi tracing framework
 * generator registry
 * kernel support
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_proc.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_rwlock.h>
#include <trace/directory.h>
#include <trace/common/options.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/KERNEL/gen_rgy.h>
#include <trace/KERNEL/gen_rgy_impl.h>
#include <trace/KERNEL/postmaster.h>
#include <trace/KERNEL/postmaster_impl.h>

#define OSI_TRACE_GEN_RGY_PID_HASH_SIZE 7   /* should be prime */

#define OSI_TRACE_GEN_RGY_PID_HASH_FUNC(pid) \
    (pid % OSI_TRACE_GEN_RGY_PID_HASH_SIZE)

struct osi_trace_gen_rgy_pid_hash {
    osi_list_head_volatile chain;
    osi_uint32 osi_volatile chain_len;
    osi_rwlock_t chain_lock;
};

struct osi_trace_gen_rgy_ptype_list {
    osi_list_head_volatile list;
    osi_uint32 osi_volatile list_len;
};

osi_mem_object_cache_t * osi_trace_gen_rgy_cache = osi_NULL;
osi_mem_object_cache_t * osi_trace_gen_rgy_hold_cache = osi_NULL;

#if ((OSI_TRACE_GEN_RGY_MAX_ID_KERNEL+1) % OSI_TYPE_FAST_BITS)
#define OSI_TRACE_GEN_RGY_INUSE_VEC_LEN \
    (((OSI_TRACE_GEN_RGY_MAX_ID_KERNEL+1)/OSI_TYPE_FAST_BITS)+1)
#else
#define OSI_TRACE_GEN_RGY_INUSE_VEC_LEN \
    ((OSI_TRACE_GEN_RGY_MAX_ID_KERNEL+1)/OSI_TYPE_FAST_BITS)
#endif

#define OSI_TRACE_GEN_RGY_INUSE_MASK (OSI_TYPE_FAST_BITS-1)
#define OSI_TRACE_GEN_RGY_INUSE_SHIFT (OSI_TYPE_FAST_LOG2_BITS)

struct osi_trace_gen_rgy {
    /*
     * BEGIN sync block
     * the following fields are synchronized by osi_trace_gen_rgy.lock
     */
    osi_trace_generator_registration_t * osi_volatile gen[OSI_TRACE_GEN_RGY_MAX_ID_KERNEL];
    struct osi_trace_gen_rgy_ptype_list gen_by_ptype[osi_ProgramType_Max_Id];
    osi_fast_uint gen_inuse[OSI_TRACE_GEN_RGY_INUSE_VEC_LEN];
    osi_list_head_volatile hold_cleanup_list;
    /*
     * END sync block
     */

    /*
     * BEGIN sync block
     * the following fields are synchronized internally
     */
    struct osi_trace_gen_rgy_pid_hash gen_pid_hash[OSI_TRACE_GEN_RGY_PID_HASH_SIZE];
    /*
     * END sync block
     */

    osi_rwlock_t lock;
};

struct osi_trace_gen_rgy osi_trace_gen_rgy;

typedef osi_uint32 osi_trace_gen_rgy_idx_t;

#define OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE 0
#define OSI_TRACE_GEN_RGY_LOOKUP_ALLOW_OFFLINE 1


/* static prototypes */
osi_static int 
osi_trace_gen_rgy_ctor(void * buf, void * sdata, int flags);
osi_static void 
osi_trace_gen_rgy_dtor(void * buf, void * sdata);
osi_static osi_inline osi_result
osi_trace_gen_rgy_idx(osi_trace_gen_id_t gen_id,
		      osi_trace_gen_rgy_idx_t * idx_out);
osi_static osi_inline osi_result
osi_trace_gen_rgy_inuse_index(osi_trace_gen_rgy_idx_t id,
			      osi_uint32 * index, 
			      osi_uint32 * bitnum);
osi_static osi_result
osi_trace_gen_rgy_find_zero_bit(osi_fast_uint bitvec, osi_uint32 * bitnum_out);
osi_static osi_result
osi_trace_gen_rgy_inuse_alloc(osi_trace_gen_rgy_idx_t * id_out);
osi_static osi_inline osi_result
osi_trace_gen_rgy_inuse_mark_r(osi_trace_gen_rgy_idx_t id);
osi_static osi_inline osi_result
osi_trace_gen_rgy_inuse_unmark_r(osi_trace_gen_rgy_idx_t id);
osi_static osi_inline void
osi_trace_gen_rgy_ptype_list_add_r(osi_trace_generator_registration_t * gen);
osi_static osi_inline void
osi_trace_gen_rgy_ptype_list_del_r(osi_trace_generator_registration_t * gen);
osi_static osi_result
osi_trace_gen_rgy_register_internal(osi_trace_generator_registration_t * gen,
				    osi_trace_gen_id_t * gen_id_out);
osi_static osi_result
osi_trace_gen_rgy_unregister_internal(osi_trace_generator_registration_t * gen);
osi_static osi_result
osi_trace_gen_rgy_alloc(osi_trace_generator_registration_t ** gen_out);
osi_static osi_result
__osi_trace_gen_rgy_really_free(void * buf);
osi_static osi_result 
osi_trace_gen_rgy_register_notify(osi_trace_generator_registration_t * gen);
osi_static osi_result 
osi_trace_gen_rgy_unregister_notify(osi_trace_generator_registration_t * gen);
osi_static void
osi_trace_gen_rgy_pid_hash_add(osi_trace_generator_registration_t * gen);
osi_static void
osi_trace_gen_rgy_pid_hash_del(osi_trace_generator_registration_t * gen);
osi_static osi_result
osi_trace_gen_rgy_lookup(osi_trace_gen_id_t gen_id,
			 osi_trace_generator_registration_t ** gen_out,
			 int allow_offline);
osi_static osi_result
osi_trace_gen_rgy_lookup_by_pid(osi_uint32 pid, 
				osi_trace_generator_registration_t ** gen_out,
				int allow_offline);
osi_static osi_result
osi_trace_gen_rgy_lookup_by_ptype(osi_uint32 ptype,
				  osi_trace_generator_registration_t ** gen_out,
				  int allow_offline);
osi_static osi_result
osi_trace_gen_rgy_lookup_by_addr(osi_trace_generator_address_t * gen_addr,
				 osi_trace_generator_registration_t ** gen_out,
				 int allow_offline);
osi_static osi_result
osi_trace_gen_rgy_hold_add(osi_trace_generator_registration_t * holder,
			   osi_trace_generator_registration_t * holdee);
osi_static osi_result
osi_trace_gen_rgy_hold_del(osi_trace_generator_registration_t * holder,
			   osi_trace_generator_registration_t * holdee);
osi_static osi_result
osi_trace_gen_rgy_hold_bulk_free_r(osi_trace_generator_registration_t *);
osi_static osi_result
osi_trace_gen_rgy_hold_gc(void);

/*
 * generator registration mem object constructor
 */
osi_static int
osi_trace_gen_rgy_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_generator_registration_t * gen = buf;

    osi_mutex_Init(&gen->lock, &osi_trace_common_options.mutex_opts);
    osi_refcnt_init(&gen->refcnt, 0);
    osi_trace_mailbox_init(&gen->mailbox);
    gen->id = 0;
    gen->state = OSI_TRACE_GEN_STATE_INVALID;
    osi_trace_mailbox_link(&gen->mailbox, gen);
    osi_list_Init_Head(&gen->holds);
    osi_list_Init_Element(gen,
			  osi_trace_generator_registration_t,
			  rgy_list);
    osi_list_Init_Element(gen,
			  osi_trace_generator_registration_t,
			  ptype_list);
    osi_list_Init_Element(gen,
			  osi_trace_generator_registration_t,
			  pid_hash);

    return 0;
}

/*
 * generator registration mem object destructor
 */
osi_static void
osi_trace_gen_rgy_dtor(void * buf, void * sdata)
{
    osi_trace_generator_registration_t * gen = buf;

    osi_trace_mailbox_unlink(&gen->mailbox);
    osi_trace_mailbox_destroy(&gen->mailbox);
    osi_refcnt_destroy(&gen->refcnt);
    osi_mutex_Destroy(&gen->lock);
    gen->id = 0;
    gen->state = OSI_TRACE_GEN_STATE_INVALID;
}

/*
 * add a gen to the program type list
 *
 * [IN] gen  -- gen object pointer
 */
osi_static osi_inline void
osi_trace_gen_rgy_ptype_list_add_r(osi_trace_generator_registration_t * gen)
{
    if (gen->addr.programType < osi_ProgramType_Max_Id) {
	osi_list_Append(&osi_trace_gen_rgy.gen_by_ptype[gen->addr.programType].list,
			gen, osi_trace_generator_registration_t, ptype_list);
	osi_trace_gen_rgy.gen_by_ptype[gen->addr.programType].list_len++;
    }
}

/*
 * remove a gen from the program type list
 *
 * [IN] gen  -- gen object pointer
 */
osi_static osi_inline void
osi_trace_gen_rgy_ptype_list_del_r(osi_trace_generator_registration_t * gen)
{
    if (osi_list_IsOnList(gen, osi_trace_generator_registration_t, ptype_list)) {
	osi_list_Remove(gen, osi_trace_generator_registration_t, ptype_list);
	osi_trace_gen_rgy.gen_by_ptype[gen->addr.programType].list_len--;
    }
}

/*
 * compute the gen_rgy array index from a gen id
 *
 * [IN] gen_id    -- generator id
 * [OUT] idx_out  -- gen_rgy array index
 *
 * returns:
 *   OSI_OK on valid gen_id
 *   OSI_FAIL on invalid gen_id
 */
osi_static osi_inline osi_result
osi_trace_gen_rgy_idx(osi_trace_gen_id_t gen_id,
		      osi_trace_gen_rgy_idx_t * idx_out)
{
    osi_result res = OSI_OK;

    *idx_out = gen_id - 1;
    if (osi_compiler_expect_false((gen_id == OSI_TRACE_GEN_RGY_KERNEL_ID) &&
				  (gen_id > OSI_TRACE_GEN_RGY_MAX_ID_KERNEL))) {
	res = OSI_FAIL;
    }

    return res;
}

/*
 * compute index and bitnum for inuse vec
 *
 * [IN] id       -- gen rgy idx
 * [OUT] index   -- inuse array index
 * [OUT] bitnum  -- bit number
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_inline osi_result
osi_trace_gen_rgy_inuse_index(osi_trace_gen_rgy_idx_t id,
			      osi_uint32 * index, 
			      osi_uint32 * bitnum)
{
    osi_uint32 offset;

    *bitnum = offset = id & OSI_TRACE_GEN_RGY_INUSE_MASK;
    *index = offset >> OSI_TRACE_GEN_RGY_INUSE_SHIFT;

    return OSI_OK;
}

/*
 * zero bit binary search algorithm
 *
 * [IN] bitvec       -- bit vector
 * [OUT] bitnum_out  -- first zero bit number
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on all-ones vector
 */
osi_static osi_fast_uint mask_vec[8] =
    {
#if (OSI_TYPE_FAST_BITS == 64)
	OSI_TYPE_UINT32_MAX,
#endif
	OSI_TYPE_UINT16_MAX,
	OSI_TYPE_UINT8_MAX,
	0xf,
	0x3,
	0x1,
	0
    };
osi_static osi_uint32 shift_vec[8] =
    {
#if (OSI_TYPE_FAST_BITS == 64)
	OSI_TYPE_INT32_BITS,
#endif
	OSI_TYPE_INT16_BITS,
	OSI_TYPE_INT8_BITS,
	4,
	2,
	1,
	0,
	0,
    };
osi_static osi_result
osi_trace_gen_rgy_find_zero_bit(osi_fast_uint bitvec, osi_uint32 * bitnum_out)
{
    osi_fast_uint mask;
    osi_uint32 idx, shift, bitnum;
    
    bitnum = 0;
    for (idx = 0, mask = mask_vec[idx], shift = shift_vec[idx]; 
	 shift > 1; 
	 idx++, mask = mask_vec[idx], shift = shift_vec[idx]) {
	if ((bitvec & mask) != mask) {
	    continue;
	} else {
	    bitnum += shift;
	    bitvec >>= shift;
	    if ((bitvec & mask) == mask) {
		return OSI_FAIL;
	    }
	}
    }

    if (bitvec & 0x1) {
	bitnum++;
    }
    *bitnum_out = bitnum;

    return OSI_OK;
}

/*
 * allocate an inuse element
 *
 * [OUT] id_out
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on gen_id exhaustion
 */
osi_static osi_result
osi_trace_gen_rgy_inuse_alloc(osi_trace_gen_rgy_idx_t * id_out)
{
    osi_result res = OSI_FAIL;
    osi_trace_gen_rgy_idx_t id;
    osi_uint32 index, bitnum;
    osi_fast_uint inuse;

    for (index = 0, id = 0; 
	 index < OSI_TRACE_GEN_RGY_INUSE_VEC_LEN; 
	 index++, id += OSI_TYPE_FAST_BITS) {
	inuse = osi_trace_gen_rgy.gen_inuse[index];
	if (inuse != OSI_TYPE_FAST_UINT_MAX) {
	    res = osi_trace_gen_rgy_find_zero_bit(inuse, &bitnum);
	    if (OSI_RESULT_OK_LIKELY(res)) {
		osi_trace_gen_rgy.gen_inuse[index] |= (1 << bitnum);
		id += bitnum;
		res = OSI_OK;
	    }
	    break;
	}
    }
    *id_out = id;

    return res;
}

/*
 * mark a gen id in use
 *
 * [IN] id
 *
 * returns:
 *   see:
 *      osi_trace_gen_rgy_inuse_index
 */
osi_static osi_inline osi_result
osi_trace_gen_rgy_inuse_mark_r(osi_trace_gen_rgy_idx_t id)
{
    osi_result res;
    osi_uint32 index, bitnum;

    res = osi_trace_gen_rgy_inuse_index(id, &index, &bitnum);
    osi_trace_gen_rgy.gen_inuse[index] |= (1 << bitnum);

    return res;
}

/*
 * mark a gen id not in use
 *
 * [IN] id
 *
 * returns:
 *   see:
 *      osi_trace_gen_rgy_inuse_index
 */
osi_static osi_inline osi_result
osi_trace_gen_rgy_inuse_unmark_r(osi_trace_gen_rgy_idx_t id)
{
    osi_result res;
    osi_uint32 index, bitnum;

    res = osi_trace_gen_rgy_inuse_index(id, &index, &bitnum);
    osi_trace_gen_rgy.gen_inuse[index] &= ~(1 << bitnum);

    return res;
}

/*
 * allocate a gen id and register gen with core data structures
 *
 * [IN] gen          -- gen object pointer
 * [OUT] gen_id_out  -- generator id
 *
 * postconditions:
 *   gen added to the gen vector, ptype list, and pid hash
 *
 * returns:
 *   see:
 *      osi_trace_gen_rgy_inuse_alloc
 */
osi_static osi_result
osi_trace_gen_rgy_register_internal(osi_trace_generator_registration_t * gen,
				    osi_trace_gen_id_t * gen_id_out)
{
    osi_result res;
    osi_trace_gen_id_t gen_id;
    osi_trace_gen_rgy_idx_t idx;

    osi_rwlock_WrLock(&osi_trace_gen_rgy.lock);
    res = osi_trace_gen_rgy_inuse_alloc(&idx);
    if (OSI_RESULT_FAIL(res)) {
	osi_rwlock_Unlock(&osi_trace_gen_rgy.lock);
	goto idx_exhausted;
    }
    gen_id = idx + 1;
    *gen_id_out = gen->id = gen_id;
    gen->state = OSI_TRACE_GEN_STATE_ONLINE;
    osi_trace_gen_rgy.gen[idx] = gen;
    osi_trace_gen_rgy_ptype_list_add_r(gen);
    osi_rwlock_Unlock(&osi_trace_gen_rgy.lock);

    osi_trace_gen_rgy_pid_hash_add(gen);

    (void)osi_trace_mailbox_gen_id_set(&gen->mailbox, gen_id);

 idx_exhausted:
    return res;
}

/*
 * unregister a gen from the core data structures
 *
 * [IN] gen          -- gen object pointer
 *
 * preconditions:
 *   gen->state is online
 *
 * postconditions:
 *   gen->state set to offline
 *   mailbox gen id association destroyed
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL if preconditions not met
 */
osi_static osi_result
osi_trace_gen_rgy_unregister_internal(osi_trace_generator_registration_t * gen)
{
    osi_result res = OSI_OK;

    osi_mutex_Lock(&gen->lock);
    if (gen->state != OSI_TRACE_GEN_STATE_ONLINE) {
	goto racing;
    }
    gen->state = OSI_TRACE_GEN_STATE_OFFLINE;
    osi_mutex_Unlock(&gen->lock);

    (void)osi_trace_mailbox_gen_id_set(&gen->mailbox, OSI_TRACE_GEN_RGY_KERNEL_ID);

 done:
    return res;

 racing:
    osi_mutex_Unlock(&gen->lock);
    res = OSI_FAIL;
    goto done;
}

/*
 * allocate a gen object
 *
 * [OUT] gen_out  -- pointer to gen object
 *
 * postconditions:
 *   gen object returned with refcnt 1
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_trace_gen_rgy_alloc(osi_trace_generator_registration_t ** gen_out)
{
    osi_result res = OSI_OK;
    osi_trace_generator_registration_t * gen;

    *gen_out = gen = (osi_trace_generator_registration_t *)
	osi_mem_object_cache_alloc(osi_trace_gen_rgy_cache);
    if (osi_compiler_expect_false(gen == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_refcnt_reset(&gen->refcnt, 1);

 error:
    return res;
}


/*
 * theory behind gen object deallocation
 *
 *
 * gen objects cannot be freed without a great deal of care.
 * we use refcounting to determine whether or not it is safe
 * to free a gen.  refs can be held by the postmaster, internal
 * gen_rgy data structures, and userspace contexts.  whenever
 * a ref is put back, the following loop happens:
 *
 * 1) atomically decrement gen->refcount and compare new value to zero
 *    if the value is NOT zero goto 2, else continue on to 1.1:
 * 1.1)   acquire osi_trace_gen_rgy.lock exclusively
 * 1.2)   atomically increment gen->refcount and compare new value to one
 *        if the value is NOT one goto 1.3, else continue on to 1.2.1:
 * 1.2.1)     mark the corresponding inuse vector bit zero
 * 1.2.2)     free the gen object back to the mem_object_cache
 * 1.2.3)     goto 2
 *
 * 1.3)   release osi_trace_gen_rgy.lock
 * 1.4)   goto 1
 *
 * 2) terminate on success code OSI_OK
 *
 */

/*
 * internal function to really free a truly zero ref'd gen
 *
 * [IN] buf  -- opaque pointer to gen object
 *
 * preconditions:
 *   osi_trace_gen_rgy.lock held exclusively
 *
 * postconditions:
 *   osi_trace_gen_rgy.lock NOT held
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
__osi_trace_gen_rgy_really_free(void * buf)
{
    osi_result res = OSI_OK;
    osi_trace_gen_rgy_idx_t idx;
    osi_trace_generator_registration_t * gen = buf;

    if (OSI_RESULT_OK(osi_trace_gen_rgy_idx(gen->id, &idx))) {
	osi_trace_gen_rgy_inuse_unmark_r(idx);
	osi_trace_gen_rgy.gen[idx] = osi_NULL;
    }

    osi_trace_gen_rgy_ptype_list_del_r(gen);
    osi_trace_gen_rgy_pid_hash_del(gen);
    osi_trace_gen_rgy_hold_bulk_free_r(gen);

    osi_rwlock_Unlock(&osi_trace_gen_rgy.lock);

    osi_mem_object_cache_free(osi_trace_gen_rgy_cache, gen);

    return res;
}

/*
 * internal function to attempt freeing a zero ref'd gen
 *
 * [IN] buf  -- opaque pointer to gen object
 *
 * preconditions:
 *   caller MUST NOT hold any gen_rgy locks
 *
 * returns:
 *   OSI_OK if we successfully destroyed the object
 *   OSI_FAIL if we're racing another thread
 */
osi_result
__osi_trace_gen_rgy_free(void * buf)
{
    osi_result res;
    int code;
    osi_trace_generator_registration_t * gen = buf;

    osi_rwlock_WrLock(&osi_trace_gen_rgy.lock);
    code = osi_refcnt_inc_action(&gen->refcnt,
				 1,
				 &__osi_trace_gen_rgy_really_free,
				 gen,
				 &res);
    if (code) {
	/* really free action fired */
	res = OSI_OK;
    } else {
	/* we raced another thread, and really free didn't fire */
	osi_rwlock_Unlock(&osi_trace_gen_rgy.lock);
	res = OSI_FAIL;
    }

    return (code) ? OSI_OK : OSI_FAIL;
}

/*
 * notify consumers of a new gen id coming online
 *
 * [IN] gen  -- gen object pointer
 *
 * returns:
 *   OSI_OK on success
 *   see:
 *      osi_trace_mail_msg_alloc,
 *      osi_trace_mail_prepare_send,
 *      osi_trace_mail_send
 */
osi_static osi_result 
osi_trace_gen_rgy_register_notify(osi_trace_generator_registration_t * gen)
{
    osi_trace_mail_msg_gen_up_t * req;
    osi_trace_mail_message_t * msg = osi_NULL;
    osi_result code;

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_gen_up_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_gen_up_t * ) msg->body;
    req->spares[0] = 0;
    req->spares[1] = 0;
    req->flags = 0;
    req->gen_state = 0;
    req->gen_id = (osi_uint32) gen->id;
    req->gen_type = (osi_uint16) gen->addr.programType;
    req->gen_pid = (osi_uint32) gen->addr.pid;

    code = osi_trace_mail_prepare_send(msg, 
				       OSI_TRACE_GEN_RGY_MCAST_CONSUMER, 
				       0, 
				       0, 
				       OSI_TRACE_MAIL_MSG_GEN_UP);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto cleanup;
    }

    code = osi_trace_mail_send(msg);
    (void)osi_trace_mail_msg_put(msg);

 error:
    return code;

 cleanup:
    if (msg != osi_NULL) {
	(void)osi_trace_mail_msg_put(msg);
    }
    goto error;
}

/*
 * notify consumers of a gen going offline
 *
 * [IN] gen  -- gen object pointer
 *
 * returns:
 *   OSI_OK on success
 *   see:
 *      osi_trace_mail_msg_alloc,
 *      osi_trace_mail_prepare_send,
 *      osi_trace_mail_send
 */
osi_static osi_result 
osi_trace_gen_rgy_unregister_notify(osi_trace_generator_registration_t * gen)
{
    osi_trace_mail_msg_gen_down_t * req;
    osi_trace_mail_message_t * msg = osi_NULL;
    osi_result code;

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_gen_down_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_gen_down_t * ) msg->body;
    req->spares[0] = 0;
    req->spares[1] = 0;
    req->flags = 0;
    req->gen_state = 0;
    req->gen_id = (osi_uint32) gen->id;
    req->gen_type = (osi_uint16) gen->addr.programType;
    req->gen_pid = (osi_uint32) gen->addr.pid;

    code = osi_trace_mail_prepare_send(msg,
				       OSI_TRACE_GEN_RGY_MCAST_CONSUMER,
				       0,
				       0,
				       OSI_TRACE_MAIL_MSG_GEN_DOWN);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto cleanup;
    }

    code = osi_trace_mail_send(msg);
    (void)osi_trace_mail_msg_put(msg);

 error:
    return code;

 cleanup:
    if (msg != osi_NULL) {
	(void)osi_trace_mail_msg_put(msg);
    }
    goto error;
}

/*
 * register a gen onto the pid hash
 *
 * [IN] gen  -- gen object pointer
 *
 */
osi_static void
osi_trace_gen_rgy_pid_hash_add(osi_trace_generator_registration_t * gen)
{
    int hash;

    hash = OSI_TRACE_GEN_RGY_PID_HASH_FUNC(gen->addr.pid);
    osi_rwlock_WrLock(&osi_trace_gen_rgy.gen_pid_hash[hash].chain_lock);
    osi_list_Append(&osi_trace_gen_rgy.gen_pid_hash[hash].chain, 
		    gen, 
		    osi_trace_generator_registration_t, 
		    pid_hash);
    osi_trace_gen_rgy.gen_pid_hash[hash].chain_len++;
    osi_rwlock_Unlock(&osi_trace_gen_rgy.gen_pid_hash[hash].chain_lock);
}

/*
 * delete a gen from the pid hash
 *
 * [IN] gen  -- gen object pointer
 *
 */
osi_static void
osi_trace_gen_rgy_pid_hash_del(osi_trace_generator_registration_t * gen)
{
    int hash;

    hash = OSI_TRACE_GEN_RGY_PID_HASH_FUNC(gen->addr.pid);
    osi_rwlock_WrLock(&osi_trace_gen_rgy.gen_pid_hash[hash].chain_lock);
    if (osi_list_IsOnList(gen, osi_trace_generator_registration_t, pid_hash)) {
	osi_list_Remove(gen, 
			osi_trace_generator_registration_t, 
			pid_hash);
	osi_trace_gen_rgy.gen_pid_hash[hash].chain_len--;
    }
    osi_rwlock_Unlock(&osi_trace_gen_rgy.gen_pid_hash[hash].chain_lock);
}

/*
 * lookup a generator registration by gen id
 *
 * [IN] gen_id         -- generator id
 * [OUT] gen_out       -- gen object pointer
 * [IN] allow_offline  -- allow lookup of an offline gen
 *
 * postconditions:
 *   ref held on gen
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on failure
 */
osi_static osi_result
osi_trace_gen_rgy_lookup(osi_trace_gen_id_t gen_id,
			 osi_trace_generator_registration_t ** gen_out,
			 int allow_offline)
{
    osi_result res;
    osi_uint32 idx;
    osi_trace_generator_registration_t * gen;

    res = osi_trace_gen_rgy_idx(gen_id, &idx);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = OSI_FAIL;
    osi_rwlock_RdLock(&osi_trace_gen_rgy.lock);
    *gen_out = gen = osi_trace_gen_rgy.gen[idx];
    if (osi_compiler_expect_true(gen != osi_NULL)) {
	osi_mutex_Lock(&gen->lock);
	if (allow_offline || (gen->state == OSI_TRACE_GEN_STATE_ONLINE)) {
	    osi_trace_gen_rgy_get(gen);
	    res = OSI_OK;
	}
	osi_mutex_Unlock(&gen->lock);
    }
    osi_rwlock_Unlock(&osi_trace_gen_rgy.lock);

 error:
    return res;
}

/*
 * lookup a gen object by its pid
 *
 * [IN] pid            -- pid
 * [OUT] gen_out       -- gen object pointer
 * [IN] allow_offline  -- allow lookup of an offline gen
 *
 * postconditions:
 *   ref held on gen
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on lookup failure
 */
osi_static osi_result
osi_trace_gen_rgy_lookup_by_pid(osi_uint32 pid, 
				osi_trace_generator_registration_t ** gen_out,
				int allow_offline)
{
    osi_result res = OSI_FAIL;
    osi_trace_generator_registration_t * gen;
    int hash;

    hash = OSI_TRACE_GEN_RGY_PID_HASH_FUNC(pid);
    osi_rwlock_RdLock(&osi_trace_gen_rgy.gen_pid_hash[hash].chain_lock);
    for (osi_list_Scan_Immutable(&osi_trace_gen_rgy.gen_pid_hash[hash].chain,
				 gen, 
				 osi_trace_generator_registration_t, 
				 pid_hash)) {
	if (gen->addr.pid == pid) {
	    osi_mutex_Lock(&gen->lock);
	    if (allow_offline || (gen->state == OSI_TRACE_GEN_STATE_ONLINE)) {
		*gen_out = gen;
		osi_trace_gen_rgy_get(gen);
		res = OSI_OK;
	    }
	    osi_mutex_Unlock(&gen->lock);
	    break;
	}
    }
    osi_rwlock_Unlock(&osi_trace_gen_rgy.gen_pid_hash[hash].chain_lock);

    return res;
}

/*
 * lookup a gen by program type
 *
 * [IN] ptype          -- program type
 * [OUT] gen_out       -- gen object pointer
 * [IN] allow_offline  -- allow lookup of an offline gen
 *
 * postconditions:
 *   ref held on gen object
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on lookup failure
 */
osi_static osi_result
osi_trace_gen_rgy_lookup_by_ptype(osi_uint32 ptype,
				  osi_trace_generator_registration_t ** gen_out,
				  int allow_offline)
{
    osi_result res = OSI_FAIL;
    osi_trace_generator_registration_t * gen;

    osi_rwlock_RdLock(&osi_trace_gen_rgy.lock);
    if (osi_trace_gen_rgy.gen_by_ptype[ptype].list_len == 1) {
	gen = osi_list_First(&osi_trace_gen_rgy.gen_by_ptype[ptype].list,
			     osi_trace_generator_registration_t, 
			     ptype_list);
	osi_mutex_Lock(&gen->lock);
	if (allow_offline || (gen->state == OSI_TRACE_GEN_STATE_ONLINE)) {
	    *gen_out = gen;
	    osi_trace_gen_rgy_get(gen);
	    res = OSI_OK;
	}
	osi_mutex_Unlock(&gen->lock);
    }
    osi_rwlock_Unlock(&osi_trace_gen_rgy.lock);

    return res;
}

/*
 * lookup a gen by address
 *
 * [IN] gen_addr       -- generator address
 * [OUT] gen_out       -- gen object pointer
 * [IN] allow_offline  -- allow lookup of an offline gen
 *
 * postconditions:
 *   ref held on gen object
 *
 * returns:
 *   OSI_FAIL on invalid address
 *   see:
 *      osi_trace_gen_rgy_lookup_by_pid
 *      osi_trace_gen_rgy_lookup_by_ptype
 */
osi_static osi_result
osi_trace_gen_rgy_lookup_by_addr(osi_trace_generator_address_t * gen_addr,
				 osi_trace_generator_registration_t ** gen_out,
				 int allow_offline)
{
    osi_result res;

    if (gen_addr->pid) {
	res = osi_trace_gen_rgy_lookup_by_pid(gen_addr->pid, 
					      gen_out,
					      allow_offline);
    } else if (gen_addr->programType) {
	res = osi_trace_gen_rgy_lookup_by_ptype(gen_addr->programType, 
						gen_out,
						allow_offline);
    } else {
	res = OSI_FAIL;
    }

    return res;
}

/*
 * lookup a mailbox object by gen id
 *
 * [IN] gen_id     -- generator id
 * [OUT] mbox_out  -- mailbox object pointer
 *
 * postconditions:
 *   ref held on gen
 *
 * returns:
 *   see:
 *      osi_trace_gen_rgy_lookup
 */
osi_result
osi_trace_gen_rgy_mailbox_get(osi_trace_gen_id_t gen_id,
			      osi_trace_mailbox_t ** mbox_out)
{
    osi_result res;
    osi_trace_generator_registration_t * gen;

    res = osi_trace_gen_rgy_lookup(gen_id, 
				   &gen,
				   OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE);
    if (OSI_RESULT_OK_LIKELY(res)) {
	*mbox_out = &gen->mailbox;
    }
    return res;
}

/*
 * resolve a gen address from a gen id
 *
 * [IN] gen_id     -- generator id
 * [OUT] gen_addr  -- generator address
 *
 * returns:
 *   see:
 *      osi_trace_gen_rgy_lookup
 *      osi_trace_gen_rgy_put
 */
osi_result
osi_trace_gen_rgy_lookup_I2A(osi_trace_gen_id_t gen_id,
			     osi_trace_generator_address_t * gen_addr)
{
    osi_result res;
    osi_trace_generator_registration_t * gen;

    res = osi_trace_gen_rgy_lookup(gen_id, &gen,
				   OSI_TRACE_GEN_RGY_LOOKUP_ALLOW_OFFLINE);
    if (OSI_RESULT_OK_LIKELY(res)) {
	osi_mutex_Lock(&gen->lock);
	osi_mem_copy(gen_addr, 
		     &gen->addr,
		     sizeof(osi_trace_generator_address_t));
	osi_mutex_Unlock(&gen->lock);
	res = osi_trace_gen_rgy_put(gen);
    }

    return res;
}

/*
 * resolve a gen id from a gen address
 *
 * [IN] gen_addr  -- generator address
 * [OUT] gen_id   -- generator id
 *
 * returns:
 *   see:
 *      osi_trace_gen_rgy_lookup_by_addr
 *      osi_trace_gen_rgy_put
 */
osi_result
osi_trace_gen_rgy_lookup_A2I(osi_trace_generator_address_t * gen_addr,
			     osi_trace_gen_id_t * gen_id)
{
    osi_result res;
    osi_trace_generator_registration_t * gen;

    res = osi_trace_gen_rgy_lookup_by_addr(gen_addr, &gen,
					   OSI_TRACE_GEN_RGY_LOOKUP_ALLOW_OFFLINE);
    if (OSI_RESULT_OK_LIKELY(res)) {
	*gen_id = gen->id;
	res = osi_trace_gen_rgy_put(gen);
    }

    return res;
}

/*
 * add a hold on one gen by another gen
 *
 * [IN] holder  -- generator which will be holding a ref
 * [IN] holdee  -- generator which will be held
 *
 * preconditions:
 *   refs held on holder and holdee
 *
 * postconditions:
 *   refcount on holdee incremented
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_trace_gen_rgy_hold_add(osi_trace_generator_registration_t * holder,
			   osi_trace_generator_registration_t * holdee)
{
    osi_result res = OSI_OK;
    osi_trace_gen_rgy_hold_t * hold;

    hold = (osi_trace_gen_rgy_hold_t *)
	osi_mem_object_cache_alloc(osi_trace_gen_rgy_hold_cache);
    if (osi_compiler_expect_false(hold == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_trace_gen_rgy_get(holdee);
    hold->gen = holdee;

    osi_mutex_Lock(&holder->lock);
    osi_list_Append(&holder->holds,
		    hold,
		    osi_trace_gen_rgy_hold_t,
		    hold_list);
    osi_mutex_Unlock(&holder->lock);

 error:
    return res;
}

/*
 * remove a hold of one gen on another gen
 *
 * [IN] holder
 * [IN] holdee
 *
 * preconditions:
 *   refs held on holder and holdee
 *   caller MUST NOT hold any gen_rgy locks
 *
 * postconditions:
 *   holdee refcount decremented
 *
 * returns:
 *   see osi_trace_gen_rgy_put()  [note preconditions]
 */
osi_static osi_result
osi_trace_gen_rgy_hold_del(osi_trace_generator_registration_t * holder,
			   osi_trace_generator_registration_t * holdee)
{
    osi_result res = OSI_FAIL;
    osi_trace_gen_rgy_hold_t * hold, * nhold;

    osi_mutex_Lock(&holder->lock);
    for (osi_list_Scan(&holder->holds,
		       hold, nhold,
		       osi_trace_gen_rgy_hold_t,
		       hold_list)) {
	if (hold->gen == holdee) {
	    goto found;
	}
    }
    osi_mutex_Unlock(&holder->lock);
    goto done;

 found:
    osi_list_Remove(hold,
		    osi_trace_gen_rgy_hold_t,
		    hold_list);
    osi_mutex_Unlock(&holder->lock);
    res = osi_trace_gen_rgy_put(holdee);
    osi_mem_object_cache_free(osi_trace_gen_rgy_hold_cache,
			      hold);

 done:
    return res;
}

/*
 * bulk remove a bunch of holds
 *
 * [IN] gen  -- point to gen object from which all holds will be freed
 *
 * preconditions:
 *   osi_trace_gen_rgy.lock held exclusively
 *   gen lock NOT held
 *
 * postconditions:
 *   hold list atomically pivoted onto gc list
 *
 * returns:
 *   see osi_trace_gen_rgy_put()
 */
osi_static osi_result
osi_trace_gen_rgy_hold_bulk_free_r(osi_trace_generator_registration_t * gen)
{
    osi_result res = OSI_OK;

    osi_mutex_Lock(&gen->lock);
    osi_list_SpliceAppend(&osi_trace_gen_rgy.hold_cleanup_list,
			  &gen->holds);
    osi_mutex_Unlock(&gen->lock);

    return res;
}

/*
 * GC previously freed holds
 *
 * preconditions:
 *   caller MUST NOT hold any gen_rgy locks
 *
 * postconditions:
 *   previously freed holds are destroyed and refs decremented
 *
 * returns:
 *   see osi_trace_gen_rgy_put()
 */
osi_static osi_result
osi_trace_gen_rgy_hold_gc(void)
{
    osi_result res, code = OSI_OK;
    osi_trace_gen_rgy_hold_t * hold, * nhold;
    osi_list_head gc_list;

    osi_list_Init_Head(&gc_list);

    /* atomically pivot out the gc list */
    osi_rwlock_WrLock(&osi_trace_gen_rgy.lock);
    osi_list_SpliceAppend(&gc_list,
			  &osi_trace_gen_rgy.hold_cleanup_list);
    osi_rwlock_Unlock(&osi_trace_gen_rgy.lock);

    /* drop refs from the gc'd holds, and then free them */
    for (osi_list_Scan(&gc_list,
		       hold, nhold,
		       osi_trace_gen_rgy_hold_t,
		       hold_list)) {
	osi_list_Remove(hold,
			osi_trace_gen_rgy_hold_t,
			hold_list);
	res = osi_trace_gen_rgy_put(hold->gen);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
	osi_mem_object_cache_free(osi_trace_gen_rgy_hold_cache,
				  hold);
    }

    return res;
}

/*
 * register a new generator
 *
 * [IN] gen_addr  -- generator address
 * [OUT] gen_id   -- generator id
 *
 * preconditions;
 *   no gen_rgy locks held
 *
 * postconditions:
 *   ref held on generator
 *
 * returns:
 *    see:
 *       osi_trace_gen_rgy_alloc
 *       osi_trace_mailbox_open
 *       osi_trace_gen_rgy_register_internal
 */
osi_result
osi_trace_gen_rgy_register(osi_trace_generator_address_t * gen_addr, 
			   osi_trace_gen_id_t * gen_id_out)
{
    osi_result res = OSI_OK;
    osi_trace_generator_registration_t * gen;

    res = osi_trace_gen_rgy_alloc(&gen);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    osi_mem_copy(&gen->addr, gen_addr, sizeof(osi_trace_generator_address_t));

    res = osi_trace_mailbox_open(&gen->mailbox);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto cleanup;
    }

    res = osi_trace_gen_rgy_register_internal(gen, gen_id_out);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto cleanup;
    }

    (void)osi_trace_mail_bcast_add(&gen->mailbox);
    if (gen->addr.programType == osi_ProgramType_TraceCollector) {
	(void)osi_trace_mail_mcast_add(OSI_TRACE_GEN_RGY_MCAST_CONSUMER,
				       &gen->mailbox);
    } else {
	(void)osi_trace_mail_mcast_add(OSI_TRACE_GEN_RGY_MCAST_GENERATOR,
				       &gen->mailbox);
    }

    (void)osi_trace_gen_rgy_register_notify(gen);

 error:
    return res;

 cleanup:
    (void)osi_trace_mailbox_shut(&gen->mailbox);
    (void)osi_trace_mailbox_clear(&gen->mailbox);
    (void)osi_trace_gen_rgy_put(gen);
    goto error;
}

/*
 * unregister a generator
 *
 * [IN] gen_id  -- generator id
 *
 * preconditions:
 *   no gen_rgy locks held
 *
 * postconditions:
 *   ref on generator released
 *
 * returns:
 *    see:
 *       osi_trace_gen_rgy_lookup
 *       osi_trace_gen_rgy_unregister_internal
 *       osi_trace_gen_rgy_put
 */
osi_result
osi_trace_gen_rgy_unregister(osi_trace_gen_id_t gen_id)
{
    osi_result res;
    osi_trace_generator_registration_t * gen;

    /* 
     * by requiring the gen to be online, we reduce the probability a possible race where
     * multiple calls to unregister happen for the same gen.
     *
     * e.g. say a process unregisters itself during osi_PkgShutdown, and
     * subsequently crashes.  then, bosserver tries to unregister the gen
     * on the crashed proc's behalf.
     *
     * XXX bear in mind that because the lookup and the call to unregister_internal
     * do not happen atomically with respect to osi_trace_gen_rgy.lock, there is
     * still a race window in this implementation!!!  in the future, we should
     * add a check to avoid this race as well.
     */
    res = osi_trace_gen_rgy_lookup(gen_id, &gen,
				   OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_gen_rgy_unregister_internal(gen);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto cleanup;
    }

    osi_mutex_Lock(&gen->lock);
    (void)osi_trace_mail_bcast_del(&gen->mailbox);
    if (gen->addr.programType == osi_ProgramType_TraceCollector) {
	(void)osi_trace_mail_mcast_del(OSI_TRACE_GEN_RGY_MCAST_CONSUMER,
				       &gen->mailbox);
    } else {
	(void)osi_trace_mail_mcast_del(OSI_TRACE_GEN_RGY_MCAST_GENERATOR,
				       &gen->mailbox);
    }
    (void)osi_trace_mailbox_shut(&gen->mailbox);
    (void)osi_trace_mailbox_clear(&gen->mailbox);
    osi_mutex_Unlock(&gen->lock);

    (void)osi_trace_gen_rgy_unregister_notify(gen);

    /* release ref held by call to lookup */
    res = osi_trace_gen_rgy_put(gen);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    /* release ref held by initial call to register */
    res = osi_trace_gen_rgy_put(gen);

 error:
    return res;

 cleanup:
    (void)osi_trace_gen_rgy_put(gen);
    goto error;
}


osi_result
osi_trace_gen_rgy_PkgInit(void)
{
    osi_result res = OSI_OK;
    osi_uint32 i;
    osi_size_t align;

    osi_rwlock_Init(&osi_trace_gen_rgy.lock, 
		    &osi_trace_common_options.rwlock_opts);

    for (i = 0; i < OSI_TRACE_GEN_RGY_PID_HASH_SIZE; i++) {
	osi_rwlock_Init(&osi_trace_gen_rgy.gen_pid_hash[i].chain_lock,
			&osi_trace_common_options.rwlock_opts);
	osi_trace_gen_rgy.gen_pid_hash[i].chain_len = 0;
	osi_list_Init_Head(&osi_trace_gen_rgy.gen_pid_hash[i].chain);
    }

    for (i = 0; i < osi_ProgramType_Max_Id; i++) {
	osi_list_Init_Head(&osi_trace_gen_rgy.gen_by_ptype[i].list);
	osi_trace_gen_rgy.gen_by_ptype[i].list_len = 0;
    }

    for (i = 0; i < OSI_TRACE_GEN_RGY_INUSE_VEC_LEN; i++) {
	osi_trace_gen_rgy.gen_inuse[i] = 0;
    }

    for (i = 0; i < OSI_TRACE_GEN_RGY_MAX_ID_KERNEL; i++) {
	osi_trace_gen_rgy.gen[i] = osi_NULL;
    }

    if (OSI_RESULT_FAIL(osi_cache_max_alignment(&align))) {
	align = 32;
    }

    osi_trace_gen_rgy_cache =
	osi_mem_object_cache_create("osi_trace_gen_rgy",
				    sizeof(osi_trace_generator_registration_t),
				    align,
				    osi_NULL,
				    &osi_trace_gen_rgy_ctor,
				    &osi_trace_gen_rgy_dtor,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_trace_gen_rgy_cache == osi_NULL) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_list_Init_Head(&osi_trace_gen_rgy.hold_cleanup_list);

    osi_trace_gen_rgy_hold_cache =
	osi_mem_object_cache_create("osi_trace_gen_rgy_hold",
				    sizeof(osi_trace_gen_rgy_hold_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_trace_gen_rgy_hold_cache == osi_NULL) {
	res = OSI_ERROR_NOMEM;
    }

 error:
    return res;
}

osi_result
osi_trace_gen_rgy_PkgShutdown(void)
{
    osi_trace_gen_id_t gen_id;

    for (gen_id = 0; gen_id < OSI_TRACE_GEN_RGY_MAX_ID_KERNEL; gen_id++) {
	(void)osi_trace_gen_rgy_unregister(gen_id);
    }

    osi_rwlock_Destroy(&osi_trace_gen_rgy.lock);

    osi_mem_object_cache_destroy(osi_trace_gen_rgy_cache);
    osi_mem_object_cache_destroy(osi_trace_gen_rgy_hold_cache);

    return OSI_OK;
}


/* 
 * syscall interfaces
 *
 * see src/trace/syscall.h for more information
 * about function arguments and such
 */


/*
 * register a new gen context
 *
 * opcode OSI_TRACE_SYSCALL_OP_GEN_REGISTER
 */
int
osi_trace_gen_rgy_sys_register(void * p1,
			       void * p2)
{
    int code = 0;
    osi_trace_generator_address_t addr;
    osi_trace_gen_id_t id;

    osi_trace_gen_rgy_hold_gc();

    osi_kernel_copy_in(p1, &addr, sizeof(osi_trace_generator_address_t), &code);
    if (osi_compiler_expect_true(!code)) {
	if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_gen_rgy_register(&addr, &id))) {
	    code = EINVAL;
	} else {
	    osi_kernel_copy_out(&id, p2, sizeof(osi_trace_gen_id_t), &code);
	}
    }

    return code;
}

/*
 * unregister a gen context
 *
 * opcode OSI_TRACE_SYSCALL_OP_GEN_UNREGISTER
 */
int
osi_trace_gen_rgy_sys_unregister(osi_trace_gen_id_t p1)
{
    int code = 0;
    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_gen_rgy_unregister(p1))) {
	code = EINVAL;
    }
    return code;
}

/*
 * resolve a gen address from a gen id
 *
 * opcode OSI_TRACE_SYSCALL_OP_GEN_LOOKUP_I2A
 */
int
osi_trace_gen_rgy_sys_lookup_I2A(osi_trace_gen_id_t p1,
				 void * p2)
{
    int code = 0;
    osi_trace_generator_address_t addr;

    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_gen_rgy_lookup_I2A(p1, &addr))) {
	code = EINVAL;
    } else {
	osi_kernel_copy_out(&addr, p2, sizeof(osi_trace_generator_address_t), &code);
    }
    return code;
}

/*
 * resolve a gen id from a gen address
 *
 * opcode OSI_TRACE_SYSCALL_OP_GEN_LOOKUP_A2I
 */
int
osi_trace_gen_rgy_sys_lookup_A2I(void * p1,
				 void * p2)
{
    int code = 0;
    osi_trace_generator_address_t addr;
    osi_trace_gen_id_t id;

    osi_kernel_copy_in(p1, &addr, sizeof(osi_trace_generator_address_t), &code);
    if (osi_compiler_expect_true(!code)) {
	if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_gen_rgy_lookup_A2I(&addr, &id))) {
	    code = EINVAL;
	} else {
	    osi_kernel_copy_out(&id, p2, sizeof(osi_trace_gen_id_t), &code);
	}
    }
    return code;
}

/*
 * get a gen ref given a gen id
 *
 * opcode OSI_TRACE_SYSCALL_OP_GEN_GET
 */
int
osi_trace_gen_rgy_sys_get(osi_trace_gen_id_t p1)
{
    int code = 0;
    osi_result res;
    osi_trace_generator_registration_t * holder = osi_NULL, * holdee = osi_NULL;

    osi_trace_gen_rgy_hold_gc();

    /* get our gen handle */
    res = osi_trace_gen_rgy_lookup_by_pid(osi_proc_current_id(),
					  &holder,
					  OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
	goto error;
    }

    res = osi_trace_gen_rgy_lookup(p1,
				   &holdee,
				   OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
	goto cleanup;
    }

    res = osi_trace_gen_rgy_hold_add(holder, holdee);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	if (res == OSI_ERROR_NOMEM) {
	    code = EAGAIN;
	} else {
	    code = EINVAL;
	}
    }

    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_gen_rgy_put(holdee))) {
	osi_Msg_console("%s: WARNING: failed to put back ref to holdee\r\n", __osi_func__);
    }
    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_gen_rgy_put(holder))) {
	osi_Msg_console("%s: WARNING: failed to put back ref to holder\r\n", __osi_func__);
    }

 error:
    return code;

 cleanup:
    if (holder != osi_NULL) {
	osi_trace_gen_rgy_put(holder);
    }
    if (holdee != osi_NULL) {
	osi_trace_gen_rgy_put(holdee);
    }
    goto error;
}

/*
 * put back a gen ref given a gen id
 *
 * opcode OSI_TRACE_SYSCALL_OP_GEN_PUT
 */
int
osi_trace_gen_rgy_sys_put(osi_trace_gen_id_t p1)
{
    int code = 0;
    osi_result res;
    osi_trace_generator_registration_t * holder = osi_NULL, * holdee = osi_NULL;

    /* get our gen handle */
    res = osi_trace_gen_rgy_lookup_by_pid(osi_proc_current_id(),
					  &holder,
					  OSI_TRACE_GEN_RGY_LOOKUP_ALLOW_OFFLINE);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
	goto error;
    }

    res = osi_trace_gen_rgy_lookup(p1,
				   &holdee,
				   OSI_TRACE_GEN_RGY_LOOKUP_ALLOW_OFFLINE);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
	goto cleanup;
    }

    res = osi_trace_gen_rgy_hold_del(holder, holdee);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
    }
 
    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_gen_rgy_put(holdee))) {
	osi_Msg_console("%s: WARNING: failed to put back ref to holdee\r\n", __osi_func__);
    }

    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_gen_rgy_put(holder))) {
	osi_Msg_console("%s: WARNING: failed to put back ref to holder\r\n", __osi_func__);
    }

error:
    return code;

 cleanup:
    if (holder) {
	(void)osi_trace_gen_rgy_put(holder);
    }
    if (holdee) {
	(void)osi_trace_gen_rgy_put(holdee);
    }
    goto error;
}

/*
 * get a gen ref given a gen addr
 *
 * opcode OSI_TRACE_SYSCALL_OP_GEN_GET_BY_ADDR
 */
int
osi_trace_gen_rgy_sys_get_by_addr(void * p1,
				  void * p2)
{
    int code = EINVAL;
    osi_result res;
    osi_trace_generator_registration_t * gen;
    osi_trace_generator_address_t addr;

    osi_kernel_copy_in(p1, &addr, sizeof(osi_trace_generator_address_t), &code);
    if (osi_compiler_expect_true(!code)) {
	res = osi_trace_gen_rgy_lookup_by_addr(&addr, &gen,
					       OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE);
	if (OSI_RESULT_OK_LIKELY(res)) {
	    osi_kernel_copy_out(&gen->id, p2, sizeof(osi_trace_gen_id_t), &code);
	}
    }
    return code;
}
