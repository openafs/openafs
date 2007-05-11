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

#include <trace/common/trace_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_proc.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_rwlock.h>
#include <trace/directory.h>
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
    /*
     * END sync block
     */

    /*
     * BEGIN sync block
     * the following fields are synchronized by osi_trace_gen_rgy.gc_lock
     */
    osi_list_head_volatile hold_gc_list;
    osi_list_head_volatile gen_gc_list;
    osi_fast_uint osi_volatile gc_flag;
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

    osi_mutex_t gc_lock;
    osi_rwlock_t lock;
};

struct osi_trace_gen_rgy osi_trace_gen_rgy;

typedef osi_uint32 osi_trace_gen_rgy_idx_t;

/*
 * gen_rgy_lookup flags
 */
#define OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE 0
#define OSI_TRACE_GEN_RGY_LOOKUP_ALLOW_OFFLINE 1
#define OSI_TRACE_GEN_RGY_LOOKUP_DROP_GEN_LOCK 0
#define OSI_TRACE_GEN_RGY_LOOKUP_KEEP_GEN_LOCK 1


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
osi_trace_gen_rgy_alloc(osi_trace_generator_registration_t ** gen_out);
osi_static osi_result 
osi_trace_gen_rgy_register_notify(osi_trace_generator_registration_t * gen);
osi_static osi_result 
osi_trace_gen_rgy_unregister_notify(osi_trace_gen_id_t,
				    osi_trace_generator_address_t *);
osi_static void
osi_trace_gen_rgy_pid_hash_add(osi_trace_generator_registration_t * gen);
osi_static void
osi_trace_gen_rgy_pid_hash_del(osi_trace_generator_registration_t * gen);
osi_static osi_result
osi_trace_gen_rgy_lookup(osi_trace_gen_id_t gen_id,
			 osi_trace_generator_registration_t ** gen_out,
			 int allow_offline,
			 int keep_lock);
osi_static osi_result
osi_trace_gen_rgy_lookup_by_pid(osi_uint32 pid, 
				osi_trace_generator_registration_t ** gen_out,
				int allow_offline,
				int keep_lock);
osi_static osi_result
osi_trace_gen_rgy_lookup_by_ptype(osi_uint32 ptype,
				  osi_trace_generator_registration_t ** gen_out,
				  int allow_offline,
				  int keep_lock);
osi_static osi_result
osi_trace_gen_rgy_lookup_by_addr(osi_trace_generator_address_t * gen_addr,
				 osi_trace_generator_registration_t ** gen_out,
				 int allow_offline,
				 int keep_lock);
osi_static osi_result
osi_trace_gen_rgy_hold_add(osi_trace_generator_registration_t * holder,
			   osi_trace_generator_registration_t * holdee);
osi_static osi_result
osi_trace_gen_rgy_hold_del(osi_trace_generator_registration_t * holder,
			   osi_trace_generator_registration_t * holdee);
osi_static void
osi_trace_gen_rgy_hold_bulk_free_r(osi_trace_generator_registration_t *);
osi_static osi_result
osi_trace_gen_rgy_hold_gc(osi_list_head * gc_list);

osi_static osi_result
osi_trace_gen_rgy_gen_gc(osi_list_head * gc_list);

osi_static osi_result
osi_trace_gen_rgy_gc(void);


/*
 * generator registration mem object constructor
 */
osi_static int
osi_trace_gen_rgy_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_generator_registration_t * gen = buf;

    osi_mutex_Init(&gen->lock, 
		   osi_trace_impl_mutex_opts());
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
    if (osi_compiler_expect_false((gen_id == OSI_TRACE_GEN_RGY_KERNEL_ID) ||
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
 * [OUT] id_out  -- address in which to return gen rgy index
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
    /* bitmap offsets are gen ids; map to an index value */
    *id_out = id;

    return res;
}

/*
 * mark a gen id in use
 *
 * [IN] id  -- gen rgy index id
 *
 * notes:
 *   please note that this interface is used by osi_trace_gen_rgy_PkgInit()
 *   to mark invalid gen id's as in-use
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

    /* allocate a registry index */
    res = osi_trace_gen_rgy_inuse_alloc(&idx);
    if (OSI_RESULT_FAIL(res)) {
	osi_rwlock_Unlock(&osi_trace_gen_rgy.lock);
	goto idx_exhausted;
    }
    gen_id = idx + 1;
    *gen_id_out = gen->id = gen_id;

    /*
     * gen state is set to offline until all
     * registration steps have been completed 
     */
    gen->state = OSI_TRACE_GEN_STATE_OFFLINE;

    /* register with core data structures */
    osi_trace_gen_rgy.gen[idx] = gen;
    osi_trace_gen_rgy_ptype_list_add_r(gen);
    osi_rwlock_Unlock(&osi_trace_gen_rgy.lock);

    osi_trace_gen_rgy_pid_hash_add(gen);

    (void)osi_trace_mailbox_gen_id_set(&gen->mailbox, gen_id);

 idx_exhausted:
    return res;
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

    gen->refcnt = 1;

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
 * gen_rgy data structures, and userspace contexts (by way of
 * gen hold objects).  whenever a ref is put back, the following 
 * happens:
 *
 *  1) acquire gen->lock
 *  2) decrement refcount
 *  3) if refcount is nonzero, go to (12)
 *  4)  set gen->state to OSI_TRACE_GEN_STATE_GC
 *  5)  acquire osi_trace_gen_rgy.gc_lock
 *  6)  enqueue gen onto osi_trace_gen_rgy.gen_gc_list
 *  7)  pivot gen->holds onto osi_trace_gen_rgy.hold_gc_list
 *  8)  set osi_trace_gen_rgy.gc_flag to 1
 *  9)  drop gen->lock
 * 10)  release osi_trace_gen_rgy.gc_lock
 * 11)  return
 * 12) drop gen->lock
 * 13) return
 */

/*
 * internal function to enqueue zero ref'd gens onto the gc list
 *
 * [IN] gen  -- pointer to gen object
 *
 * preconditions:
 *   gen->lock held
 *
 * postconditions:
 *   gen->state set to gc
 *   gen and gen's holds placed on gc lists
 *   gen->lock dropped
 */
void
__osi_trace_gen_rgy_free(osi_trace_generator_registration_t * gen)
{
    gen->state = OSI_TRACE_GEN_STATE_GC;

    osi_mutex_Lock(&osi_trace_gen_rgy.gc_lock);
    osi_list_Append(&osi_trace_gen_rgy.gen_gc_list,
		    gen,
		    osi_trace_generator_registration_t,
		    gc_list);
    osi_trace_gen_rgy_hold_bulk_free_r(gen);
    if (!osi_trace_gen_rgy.gc_flag) {
	osi_trace_gen_rgy.gc_flag = 1;
    }

    /* 
     * ordering of the following two lock drops is extremely important;
     * if the ordering were reversed, it would be possible for another
     * CPU to race and deallocate the object before we had a chance
     * to drop gen->lock.
     */
    osi_mutex_Unlock(&gen->lock);
    osi_mutex_Unlock(&osi_trace_gen_rgy.gc_lock);
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
osi_trace_gen_rgy_unregister_notify(osi_trace_gen_id_t gen_id,
				    osi_trace_generator_address_t * addr)
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
    req->gen_id = gen_id;
    req->gen_type = (osi_uint16) addr->programType;
    req->gen_pid = (osi_uint32) addr->pid;

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
 * [IN] keep_lock      -- return with gen->lock held
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
			 int allow_offline,
			 int keep_lock)
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
    gen = osi_trace_gen_rgy.gen[idx];
    if (osi_compiler_expect_true(gen != osi_NULL)) {
	osi_mutex_Lock(&gen->lock);
	if ((gen->state != OSI_TRACE_GEN_STATE_GC) &&
	    (allow_offline || (gen->state == OSI_TRACE_GEN_STATE_ONLINE))) {
	    *gen_out = gen;
	    osi_trace_gen_rgy_get_nl(gen);
	    res = OSI_OK;
	    if (!keep_lock) {
		osi_mutex_Unlock(&gen->lock);
	    }
	} else {
	    osi_mutex_Unlock(&gen->lock);
	}
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
 * [IN] keep_lock      -- return with gen->lock held
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
				int allow_offline,
				int keep_lock)
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
	    if ((gen->state != OSI_TRACE_GEN_STATE_GC) &&
		(allow_offline || (gen->state == OSI_TRACE_GEN_STATE_ONLINE))) {
		*gen_out = gen;
		osi_trace_gen_rgy_get_nl(gen);
		res = OSI_OK;
		if (!keep_lock) {
		    osi_mutex_Unlock(&gen->lock);
		}
	    } else {
		osi_mutex_Unlock(&gen->lock);
	    }
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
 * [IN] keep_lock      -- return with gen->lock held
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
				  int allow_offline,
				  int keep_lock)
{
    osi_result res = OSI_FAIL;
    osi_trace_generator_registration_t * gen;

    osi_rwlock_RdLock(&osi_trace_gen_rgy.lock);
    if (osi_trace_gen_rgy.gen_by_ptype[ptype].list_len == 1) {
	gen = osi_list_First(&osi_trace_gen_rgy.gen_by_ptype[ptype].list,
			     osi_trace_generator_registration_t, 
			     ptype_list);
	osi_mutex_Lock(&gen->lock);
	if ((gen->state != OSI_TRACE_GEN_STATE_GC) &&
	    (allow_offline || (gen->state == OSI_TRACE_GEN_STATE_ONLINE))) {
	    *gen_out = gen;
	    osi_trace_gen_rgy_get_nl(gen);
	    res = OSI_OK;
	    if (!keep_lock) {
		osi_mutex_Unlock(&gen->lock);
	    }
	} else {
	    osi_mutex_Unlock(&gen->lock);
	}
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
 * [IN] keep_lock      -- return with gen->lock held
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
				 int allow_offline,
				 int keep_lock)
{
    osi_result res;

    if (gen_addr->pid) {
	res = osi_trace_gen_rgy_lookup_by_pid(gen_addr->pid, 
					      gen_out,
					      allow_offline,
					      keep_lock);
    } else if (gen_addr->programType) {
	res = osi_trace_gen_rgy_lookup_by_ptype(gen_addr->programType, 
						gen_out,
						allow_offline,
						keep_lock);
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
				   OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE,
				   OSI_TRACE_GEN_RGY_LOOKUP_DROP_GEN_LOCK);
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
 */
osi_result
osi_trace_gen_rgy_lookup_I2A(osi_trace_gen_id_t gen_id,
			     osi_trace_generator_address_t * gen_addr)
{
    osi_result res;
    osi_trace_generator_registration_t * gen;

    res = osi_trace_gen_rgy_lookup(gen_id, 
				   &gen,
				   OSI_TRACE_GEN_RGY_LOOKUP_ALLOW_OFFLINE,
				   OSI_TRACE_GEN_RGY_LOOKUP_KEEP_GEN_LOCK);
    if (OSI_RESULT_OK_LIKELY(res)) {
	osi_mem_copy(gen_addr, 
		     (void *)&gen->addr,
		     sizeof(osi_trace_generator_address_t));
	osi_trace_gen_rgy_put_nl(gen);
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
 */
osi_result
osi_trace_gen_rgy_lookup_A2I(osi_trace_generator_address_t * gen_addr,
			     osi_trace_gen_id_t * gen_id)
{
    osi_result res;
    osi_trace_generator_registration_t * gen;

    res = osi_trace_gen_rgy_lookup_by_addr(gen_addr, &gen,
					   OSI_TRACE_GEN_RGY_LOOKUP_ALLOW_OFFLINE,
					   OSI_TRACE_GEN_RGY_LOOKUP_KEEP_GEN_LOCK);
    if (OSI_RESULT_OK_LIKELY(res)) {
	*gen_id = gen->id;
	osi_trace_gen_rgy_put_nl(gen);
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
 *   ref held on holder
 *   ref held on holdee
 *   holder->lock is held
 *
 * postconditions:
 *   holdee ref is donated
 *   hold object on holdee is added to holder's hold list
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

    hold->gen = holdee;
    osi_list_Append(&holder->holds,
		    hold,
		    osi_trace_gen_rgy_hold_t,
		    hold_list);

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
 *   holder->lock is held
 *
 * postconditions:
 *   caller inherits a holdee ref
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL if no such hold is found
 */
osi_static osi_result
osi_trace_gen_rgy_hold_del(osi_trace_generator_registration_t * holder,
			   osi_trace_generator_registration_t * holdee)
{
    osi_result res = OSI_FAIL;
    osi_trace_gen_rgy_hold_t * hold, * nhold;

    for (osi_list_Scan(&holder->holds,
		       hold, nhold,
		       osi_trace_gen_rgy_hold_t,
		       hold_list)) {
	if (hold->gen == holdee) {
	    goto found;
	}
    }
    goto done;

 found:
    osi_list_Remove(hold,
		    osi_trace_gen_rgy_hold_t,
		    hold_list);
    osi_mem_object_cache_free(osi_trace_gen_rgy_hold_cache,
			      hold);
    res = OSI_OK;

 done:
    return res;
}

/*
 * bulk remove a bunch of holds
 *
 * [IN] gen  -- point to gen object from which all holds will be freed
 *
 * preconditions:
 *   osi_trace_gen_rgy.gc_lock held
 *
 * postconditions:
 *   hold list atomically pivoted onto gc list
 */
osi_static void
osi_trace_gen_rgy_hold_bulk_free_r(osi_trace_generator_registration_t * gen)
{
    /* 
     * we can safely pivot this without holding gen->lock because
     * this method is only called when refcount has reached zero;
     * thus implying that no other thread is touching this gen object
     */
    osi_list_SpliceAppend(&osi_trace_gen_rgy.hold_gc_list,
			  &gen->holds);
}

/*
 * GC previously freed holds
 *
 * [IN] gc_list  -- list of holds to garbage collect
 *
 * preconditions:
 *   caller MUST NOT hold any gen_rgy locks
 *
 * postconditions:
 *   previously freed holds are destroyed and refs decremented
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_trace_gen_rgy_hold_gc(osi_list_head * gc_list)
{
    osi_result code = OSI_OK;
    osi_trace_gen_rgy_hold_t * hold, * nhold;

    /* drop refs from the gc'd holds, and then free them */
    for (osi_list_Scan(gc_list,
		       hold, nhold,
		       osi_trace_gen_rgy_hold_t,
		       hold_list)) {
	osi_list_Remove(hold,
			osi_trace_gen_rgy_hold_t,
			hold_list);
	osi_trace_gen_rgy_put(hold->gen);
	osi_mem_object_cache_free(osi_trace_gen_rgy_hold_cache,
				  hold);
    }

    return code;
}

/*
 * garbage collect unreferenced gens
 *
 * [IN] gc_list  -- list head of unreferenced gens
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_trace_gen_rgy_gen_gc(osi_list_head * gc_list)
{
    osi_trace_generator_registration_t * gen, * ngen;
    osi_trace_gen_rgy_idx_t idx;

    /* make two passes.
     *
     * in pass one, make all the changes that need to
     * happen under the gen_rgy rwlock
     *
     * in pass two, update the pid hash (pid hash has
     * its own locking hierarchy), and then deallocate
     * the gen object
     */

    osi_rwlock_WrLock(&osi_trace_gen_rgy.lock);
    for (osi_list_Scan_Immutable(gc_list,
				 gen,
				 osi_trace_generator_registration_t,
				 gc_list)) {
	if (OSI_RESULT_OK(osi_trace_gen_rgy_idx(gen->id, &idx))) {
	    osi_trace_gen_rgy_inuse_unmark_r(idx);
	    osi_trace_gen_rgy.gen[idx] = osi_NULL;
	}

	osi_trace_gen_rgy_ptype_list_del_r(gen);
    }
    osi_rwlock_Unlock(&osi_trace_gen_rgy.lock);

    for (osi_list_Scan(gc_list,
		       gen, ngen,
		       osi_trace_generator_registration_t,
		       gc_list)) {
	osi_list_Remove(gen,
			osi_trace_generator_registration_t,
			gc_list);
	osi_trace_gen_rgy_pid_hash_del(gen);
	osi_mem_object_cache_free(osi_trace_gen_rgy_cache, gen);
    }

    return OSI_OK;
}

/*
 * perform gen_rgy gc cycle
 *
 * preconditions:
 *   no gen_rgy locks held
 *
 * postconditions:
 *   holds and unreferenced gens are garbage collected
 *
 * returns:
 *   see osi_trace_gen_rgy_hold_gc()
 *   see osi_trace_gen_rgy_gen_gc()
 */
osi_static osi_result
osi_trace_gen_rgy_gc(void)
{
    osi_result res, code = OSI_OK;
    osi_list_head hold_gc_list, gen_gc_list;

    if (!osi_trace_gen_rgy.gc_flag) {
	goto done;
    }

    osi_list_Init_Head(&hold_gc_list);
    osi_list_Init_Head(&gen_gc_list);

    /* atomically pivot out the gc lists */
    osi_mutex_Lock(&osi_trace_gen_rgy.gc_lock);
    osi_list_SpliceAppend(&hold_gc_list,
			  &osi_trace_gen_rgy.hold_gc_list);
    osi_list_SpliceAppend(&gen_gc_list,
			  &osi_trace_gen_rgy.gen_gc_list);
    osi_trace_gen_rgy.gc_flag = 0;
    osi_mutex_Unlock(&osi_trace_gen_rgy.gc_lock);

    /* gc the holds */
    if (osi_list_IsNotEmpty(&hold_gc_list)) {
	res = osi_trace_gen_rgy_hold_gc(&hold_gc_list);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
    }

    /* gc the gens */
    if (osi_list_IsNotEmpty(&gen_gc_list)) {
	res = osi_trace_gen_rgy_gen_gc(&gen_gc_list);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
    }

 done:
    return code;
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

    osi_mem_copy((void *)&gen->addr, gen_addr, sizeof(osi_trace_generator_address_t));

    res = osi_trace_mailbox_open(&gen->mailbox);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto cleanup;
    }

    res = osi_trace_gen_rgy_register_internal(gen, gen_id_out);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto cleanup;
    }

    /* setup mcast and bcast membership, and pivot gen online */
    osi_mutex_Lock(&gen->lock);
    (void)osi_trace_mail_bcast_add(&gen->mailbox);
    if (gen->addr.programType == osi_ProgramType_TraceCollector) {
	(void)osi_trace_mail_mcast_add(OSI_TRACE_GEN_RGY_MCAST_CONSUMER,
				       &gen->mailbox,
				       OSI_TRACE_MAIL_GEN_LOCK_HELD);
    } else {
	(void)osi_trace_mail_mcast_add(OSI_TRACE_GEN_RGY_MCAST_GENERATOR,
				       &gen->mailbox,
				       OSI_TRACE_MAIL_GEN_LOCK_HELD);
    }

    gen->state = OSI_TRACE_GEN_STATE_ONLINE;
    osi_mutex_Unlock(&gen->lock);

    (void)osi_trace_gen_rgy_register_notify(gen);


 error:
    return res;

 cleanup:
    (void)osi_trace_mailbox_shut(&gen->mailbox);
    (void)osi_trace_mailbox_clear(&gen->mailbox);
    osi_trace_gen_rgy_put(gen);
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
 *   OSI_FAIL if gen->state is not ONLINE
 *    see:
 *       osi_trace_gen_rgy_lookup
 */
osi_result
osi_trace_gen_rgy_unregister(osi_trace_gen_id_t gen_id)
{
    osi_result res;
    osi_trace_generator_registration_t * gen;
    osi_trace_generator_address_t gen_addr;

    /* 
     * by requiring the gen to be online, we reduce the probability a possible race where
     * multiple calls to unregister happen for the same gen.
     *
     * e.g. say a process unregisters itself during osi_PkgShutdown, and
     * subsequently crashes.  then, bosserver tries to unregister the gen
     * on the crashed proc's behalf.
     */
    res = osi_trace_gen_rgy_lookup(gen_id, 
				   &gen,
				   OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE,
				   OSI_TRACE_GEN_RGY_LOOKUP_KEEP_GEN_LOCK);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    /* atomically compare and swap gen state from online to offline */
    if (gen->state != OSI_TRACE_GEN_STATE_ONLINE) {
	goto cleanup;
    }
    gen->state = OSI_TRACE_GEN_STATE_OFFLINE;

    osi_mem_copy(&gen_addr, (void *)&gen->addr, sizeof(gen->addr));

    /* disassociate mailbox from this gen id */
    (void)osi_trace_mailbox_gen_id_set(&gen->mailbox, OSI_TRACE_GEN_RGY_KERNEL_ID);

    (void)osi_trace_mail_bcast_del(&gen->mailbox);
    if (gen->addr.programType == osi_ProgramType_TraceCollector) {
	(void)osi_trace_mail_mcast_del(OSI_TRACE_GEN_RGY_MCAST_CONSUMER,
				       &gen->mailbox,
				       OSI_TRACE_MAIL_GEN_LOCK_HELD);
    } else {
	(void)osi_trace_mail_mcast_del(OSI_TRACE_GEN_RGY_MCAST_GENERATOR,
				       &gen->mailbox,
				       OSI_TRACE_MAIL_GEN_LOCK_HELD);
    }
    (void)osi_trace_mailbox_shut(&gen->mailbox);
    (void)osi_trace_mailbox_clear(&gen->mailbox);

    /* release ref held by call to lookup */
    osi_trace_gen_rgy_put_nl_nz(gen);

    /* release ref held by initial call to register */
    osi_trace_gen_rgy_put_nl(gen);

    (void)osi_trace_gen_rgy_unregister_notify(gen_id, &gen_addr);

 error:
    return res;

 cleanup:
    osi_trace_gen_rgy_put_nl(gen);
    res = OSI_FAIL;
    goto error;
}


osi_result
osi_trace_gen_rgy_PkgInit(void)
{
    osi_result res = OSI_OK;
    osi_uint32 i;
    osi_size_t align;

    osi_rwlock_Init(&osi_trace_gen_rgy.lock, 
		    osi_trace_impl_rwlock_opts());

    osi_mutex_Init(&osi_trace_gen_rgy.gc_lock, 
		   osi_trace_impl_mutex_opts());

    for (i = 0; i < OSI_TRACE_GEN_RGY_PID_HASH_SIZE; i++) {
	osi_rwlock_Init(&osi_trace_gen_rgy.gen_pid_hash[i].chain_lock,
			osi_trace_impl_rwlock_opts());
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

    /* deal with only 31 out of 32 bits being valid due to gen id zero being
     * the kernel id */
    (void)osi_trace_gen_rgy_inuse_mark_r(OSI_TRACE_GEN_RGY_MAX_ID_KERNEL);

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
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_trace_gen_rgy_cache == osi_NULL) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_trace_gen_rgy.gc_flag = 0;
    osi_list_Init_Head(&osi_trace_gen_rgy.hold_gc_list);
    osi_list_Init_Head(&osi_trace_gen_rgy.gen_gc_list);

    osi_trace_gen_rgy_hold_cache =
	osi_mem_object_cache_create("osi_trace_gen_rgy_hold",
				    sizeof(osi_trace_gen_rgy_hold_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
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

    osi_mutex_Destroy(&osi_trace_gen_rgy.gc_lock);
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

    osi_trace_gen_rgy_gc();

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
    osi_trace_generator_registration_t * holder, * holdee;

    osi_trace_gen_rgy_gc();

    res = osi_trace_gen_rgy_lookup(p1,
				   &holdee,
				   OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE,
				   OSI_TRACE_GEN_RGY_LOOKUP_DROP_GEN_LOCK);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
	goto error;
    }

    /* get our gen handle */
    res = osi_trace_gen_rgy_lookup_by_pid(osi_proc_current_id(),
					  &holder,
					  OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE,
					  OSI_TRACE_GEN_RGY_LOOKUP_KEEP_GEN_LOCK);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	osi_trace_gen_rgy_put(holdee);
	code = EINVAL;
	goto error;
    }

    res = osi_trace_gen_rgy_hold_add(holder, holdee);
    osi_trace_gen_rgy_put_nl(holder);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	osi_trace_gen_rgy_put(holdee);
	if (res == OSI_ERROR_NOMEM) {
	    code = EAGAIN;
	} else {
	    code = EINVAL;
	}
    }

 error:
    return code;
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
    int code = 0;
    osi_result res;
    osi_trace_generator_registration_t * holder, * holdee;
    osi_trace_generator_address_t addr;

    osi_kernel_handle_copy_in(p1, 
			      &addr, 
			      sizeof(osi_trace_generator_address_t), 
			      code, 
			      error);

    osi_trace_gen_rgy_gc();

    res = osi_trace_gen_rgy_lookup_by_addr(&addr, 
					   &holdee,
					   OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE,
					   OSI_TRACE_GEN_RGY_LOOKUP_DROP_GEN_LOCK);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
	goto error;
    }

    /* get our gen handle */
    res = osi_trace_gen_rgy_lookup_by_pid(osi_proc_current_id(),
					  &holder,
					  OSI_TRACE_GEN_RGY_LOOKUP_REQUIRE_ONLINE,
					  OSI_TRACE_GEN_RGY_LOOKUP_KEEP_GEN_LOCK);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	osi_trace_gen_rgy_put(holdee);
	code = EINVAL;
	goto error;
    }

    osi_kernel_handle_copy_out(&holdee->id,
			       p2,
			       sizeof(osi_trace_gen_id_t),
			       code,
			       ucopy_fault);

    res = osi_trace_gen_rgy_hold_add(holder, holdee);
    osi_trace_gen_rgy_put_nl(holder);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	osi_trace_gen_rgy_put(holdee);
	if (res == OSI_ERROR_NOMEM) {
	    code = EAGAIN;
	} else {
	    code = EINVAL;
	}
    }

 error:
    return code;

 ucopy_fault:
    osi_trace_gen_rgy_put_nl(holder);
    osi_trace_gen_rgy_put(holdee);
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
    osi_trace_generator_registration_t * holder, * holdee;

    res = osi_trace_gen_rgy_lookup(p1,
				   &holdee,
				   OSI_TRACE_GEN_RGY_LOOKUP_ALLOW_OFFLINE,
				   OSI_TRACE_GEN_RGY_LOOKUP_DROP_GEN_LOCK);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
	goto error;
    }

    /* get our gen handle */
    res = osi_trace_gen_rgy_lookup_by_pid(osi_proc_current_id(),
					  &holder,
					  OSI_TRACE_GEN_RGY_LOOKUP_ALLOW_OFFLINE,
					  OSI_TRACE_GEN_RGY_LOOKUP_KEEP_GEN_LOCK);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	osi_trace_gen_rgy_put(holdee);
	code = EINVAL;
	goto error;
    }
    
    res = osi_trace_gen_rgy_hold_del(holder, holdee);
    osi_trace_gen_rgy_put_nl(holder);
    
    if (OSI_RESULT_OK_LIKELY(res)) {
	/* drop the inherited ref, and our ref from lookup */
	osi_mutex_Lock(&holdee->lock);
	osi_trace_gen_rgy_put_nl_nz(holdee);
	osi_trace_gen_rgy_put_nl(holdee);
    } else {
	osi_trace_gen_rgy_put(holdee);
	code = EINVAL;
    }

error:
    return code;
}
