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
 * inter-process asynchronous messaging
 * kernel postmaster
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_kernel.h>
#include <osi/osi_proc.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_object_cache.h>
#include <trace/syscall.h>
#include <trace/directory.h>
#include <trace/KERNEL/postmaster.h>
#include <trace/KERNEL/postmaster_impl.h>
#include <trace/KERNEL/gen_rgy.h>
#include <trace/KERNEL/gen_rgy_impl.h>

osi_static osi_result osi_trace_mail_msg_enqueue(osi_trace_mailbox_t * mbox, 
						 osi_trace_mail_message_t * msg);

#define OSI_TRACE_MAIL_MCAST_ID_COUNT  (OSI_TRACE_GEN_RGY_MCAST_MAX - OSI_TRACE_GEN_RGY_MCAST_MIN + 1)

osi_mem_object_cache_t * osi_trace_mail_mcast_cache = osi_NULL;

struct osi_trace_mail_mcast_node {
    osi_list_element_volatile mbox_list;
    osi_trace_mailbox_t * osi_volatile mbox;
};

struct osi_trace_mail_config {
    osi_list_head_volatile mcast[OSI_TRACE_MAIL_MCAST_ID_COUNT];
    osi_list_head_volatile bcast;
    osi_list_head_volatile tap;
    osi_uint32 osi_volatile mcast_usage;    /* mcast addr usage bit vector */
    osi_rwlock_t lock;
};

/* whether global mail taps are currently active */
osi_uint32 osi_volatile osi_trace_mail_tap_active = 0;

struct osi_trace_mail_config osi_trace_mail_config;

/*
 * initialize a mailbox
 *
 * [IN] mbox  -- pointer to mailbox
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_mailbox_init(osi_trace_mailbox_t * mbox)
{
    osi_result res = OSI_OK;

    osi_list_Init(&mbox->messages);
    osi_list_Init(&mbox->taps);
    mbox->len = 0;
    mbox->state = OSI_TRACE_MAILBOX_SHUT;
    mbox->gen_id = 0;
    mbox->tap_active = 0;
    osi_mutex_Init(&mbox->lock,
		   osi_trace_impl_mutex_opts());
    osi_condvar_Init(&mbox->cv,
		     osi_trace_impl_condvar_opts());

    return res;
}

/*
 * destroy a mailbox
 *
 * [IN] mbox  -- pointer to mailbox
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_mailbox_destroy(osi_trace_mailbox_t * mbox)
{
    osi_result res = OSI_OK;

    osi_trace_mailbox_shut(mbox);
    osi_trace_mailbox_clear(mbox);

    osi_condvar_Destroy(&mbox->cv);
    osi_mutex_Destroy(&mbox->lock);

    return res;
}

/*
 * link a mailbox with a generator
 *
 * [IN] mbox    -- pointer to mailbox structure
 * [IN] gen     -- pointer to generator
 * [IN] gen_id  -- generator id
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_mailbox_link(osi_trace_mailbox_t * mbox,
		       osi_trace_generator_registration_t * gen)
{
    osi_mutex_Lock(&mbox->lock);
    mbox->gen = gen;
    osi_mutex_Unlock(&mbox->lock);

    return OSI_OK;
}

/*
 * unlink a mailbox from a generator
 *
 * [IN] mbox    -- pointer to mailbox structure
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_mailbox_unlink(osi_trace_mailbox_t * mbox)
{
    osi_mutex_Lock(&mbox->lock);
    mbox->gen = osi_NULL;
    mbox->gen_id = 0;
    osi_mutex_Unlock(&mbox->lock);

    return OSI_OK;
}

/*
 * setup mailbox cached gen_id value
 *
 * [IN] mbox    -- pointer to mailbox structure
 * [IN] gen_id  -- generator id
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_mailbox_gen_id_set(osi_trace_mailbox_t * mbox,
			     osi_trace_gen_id_t gen_id)
{
    osi_mutex_Lock(&mbox->lock);
    mbox->gen_id = gen_id;
    osi_mutex_Unlock(&mbox->lock);

    return OSI_OK;
}

/*
 * put a mailbox into the open state
 *
 * [IN] mbox  -- pointer to mailbox structure
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_mailbox_open(osi_trace_mailbox_t * mbox)
{
    osi_mutex_Lock(&mbox->lock);
    mbox->state = OSI_TRACE_MAILBOX_OPEN;
    osi_mutex_Unlock(&mbox->lock);

    return OSI_OK;
}

/*
 * put a mailbox into the shut state
 *
 * [IN] mbox  -- pointer to mailbox structure
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_mailbox_shut(osi_trace_mailbox_t * mbox)
{
    osi_mutex_Lock(&mbox->lock);
    mbox->state = OSI_TRACE_MAILBOX_SHUT;
    osi_mutex_Unlock(&mbox->lock);

    return OSI_OK;
}

/*
 * clear all pending messages in mailbox
 *
 * [IN] mbox  -- pointer to mailbox structure
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_mailbox_clear(osi_trace_mailbox_t * mbox)
{
    osi_result res = OSI_OK;
    osi_trace_mail_queue_t *mq, *nmq;

    osi_mutex_Lock(&mbox->lock);
    for (osi_list_Scan(&mbox->messages, mq, nmq,
		       osi_trace_mail_queue_t, msg_list)) {
	osi_list_Remove(mq, osi_trace_mail_queue_t, msg_list);
	osi_trace_mail_msg_put(mq->msg);
	mq->msg = osi_NULL;
	osi_trace_mail_mq_free(mq);
    }
    mbox->len = 0;
    osi_mutex_Unlock(&mbox->lock);

    return res;
}

/*
 * enqueue a message onto a mailbox in queue
 *
 * [IN] mbox  -- pointer to mailbox
 * [IN] msg   -- pointer to message
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on low memory condition
 *   OSI_TRACE_MAIL_ERROR_MAILBOX_SHUT when mailbox is not open
 */
osi_static osi_result
osi_trace_mail_msg_enqueue(osi_trace_mailbox_t * mbox, osi_trace_mail_message_t * msg)
{
    osi_result res = OSI_OK;
    osi_trace_mail_queue_t * mq;

    /* allocate a queue element */
    res = osi_trace_mail_mq_alloc(&mq);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto done;
    }
    mq->msg = msg;

    /* increment the refcount */
    osi_trace_mail_msg_get(msg);

    osi_mutex_Lock(&mbox->lock);
    if (osi_compiler_expect_true(mbox->state == OSI_TRACE_MAILBOX_OPEN)) {
	/* enqueue the message */
	osi_list_Append(&mbox->messages, mq, osi_trace_mail_queue_t, msg_list);
	mbox->len++;
	if (mbox->len == 1) {
	    osi_condvar_Signal(&mbox->cv);
	}
    } else {
	res = OSI_TRACE_MAIL_ERROR_MAILBOX_SHUT;
	goto error_sync;
    }
    osi_mutex_Unlock(&mbox->lock);

 done:
    return res;

 error_sync:
    osi_mutex_Unlock(&mbox->lock);
    if (mq) {
	mq->msg = osi_NULL;
	osi_trace_mail_mq_free(mq);
    }
    osi_trace_mail_msg_put(msg);
    goto done;
}

/*
 * send a message
 *
 * [IN] msg  -- pointer to message structure
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM when a low memory condition is detected
 *   OSI_FAIL on miscellaneous failure
 */
osi_result 
osi_trace_mail_send(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, res;
    osi_trace_mailbox_t *mbox, *tap_mbox;
    struct osi_trace_mail_mcast_node * node;
    osi_trace_gen_id_t dst_id;
    osi_uint32 config_lock_held = 0;

    dst_id = msg->envelope.env_dst;

    if ((dst_id != OSI_TRACE_GEN_RGY_BCAST_ID) &&
	osi_trace_mail_tap_active) {
	/* global mail taps */
	osi_rwlock_RdLock(&osi_trace_mail_config.lock);
	config_lock_held = 1;
	for (osi_list_Scan_Immutable(&osi_trace_mail_config.tap,
				     node,
				     struct osi_trace_mail_mcast_node,
				     mbox_list)) {
	    tap_mbox = node->mbox;
	    if (dst_id != tap_mbox->gen_id) {
		(void)osi_trace_mail_msg_enqueue(tap_mbox, msg);
	    }
	}
    }

    if (dst_id >= OSI_TRACE_GEN_RGY_MCAST_MIN) {
	osi_trace_gen_id_t mcast_id;

	/* multicast/broadcast */
	mcast_id = dst_id - OSI_TRACE_GEN_RGY_MCAST_MIN;

	if (!config_lock_held) {
	    osi_rwlock_RdLock(&osi_trace_mail_config.lock);
	}

	if (dst_id == OSI_TRACE_GEN_RGY_BCAST_ID) {
	    /* broadcast */
	    for (osi_list_Scan_Immutable(&osi_trace_mail_config.bcast,
					 node, 
					 struct osi_trace_mail_mcast_node,
					 mbox_list)) {
		res = osi_trace_mail_msg_enqueue(node->mbox, msg);
		if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		    code = OSI_FAIL;
		}
	    }
	} else if (osi_trace_mail_config.mcast_usage & (1 << mcast_id)) {
	    /* multicast */
	    for (osi_list_Scan_Immutable(&osi_trace_mail_config.mcast[mcast_id],
					 node,
					 struct osi_trace_mail_mcast_node,
					 mbox_list)) {
		res = osi_trace_mail_msg_enqueue(node->mbox, msg);
		if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		    code = OSI_FAIL;
		}
	    }
	}
	osi_rwlock_Unlock(&osi_trace_mail_config.lock);
    } else {
	/* unicast */
	if (config_lock_held) {
	    osi_rwlock_Unlock(&osi_trace_mail_config.lock);
	}

	code = osi_trace_gen_rgy_mailbox_get(dst_id, &mbox);
	if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	    goto error;
	}
	code = osi_trace_mail_msg_enqueue(mbox, msg);

	if (mbox->tap_active) {
	    osi_trace_mailbox_t ** tap_vec;
	    osi_size_t i, tap_vec_len;

	    osi_mutex_Lock(&mbox->lock);
	    /* because there is no defined lock ordering between mailboxes, it
	     * is necessary for us to copy out the list of taps and drop this
	     * mailbox's lock before doing the actual message enqueues */
	    /* XXX if/when mailboxes becoming dynamically allocated we'll need
	     * to hold refs here */
	    osi_list_Count(&mbox->taps,
			   node,
			   struct osi_trace_mail_mcast_node,
			   mbox_list,
			   tap_vec_len);
	    if (osi_compiler_expect_true(tap_vec_len)) {
		tap_vec = osi_mem_alloc(tap_vec_len * sizeof(osi_trace_mailbox_t *));
		if (osi_compiler_expect_false(tap_vec == osi_NULL)) {
		    goto error_sync;
		}

		for (i = 0, osi_list_Scan_Immutable(&mbox->taps,
						    node,
						    struct osi_trace_mail_mcast_node,
						    mbox_list), i++) {
		    tap_vec[i] = node->mbox;
		}
		osi_mutex_Unlock(&mbox->lock);

		for (i = 0; i < tap_vec_len; i++) {
		    (void)osi_trace_mail_msg_enqueue(tap_vec[i], msg);
		}

		osi_mem_free(tap_vec, tap_vec_len * sizeof(osi_trace_mailbox_t *));
	    } else {
	    error_sync:
		osi_mutex_Unlock(&mbox->lock);
	    }
	}
	osi_trace_gen_rgy_mailbox_put(mbox);
    }

 error:
    return code;
}

/*
 * check for waiting messages in mailbox
 *
 * [IN] gen_id  -- generator id to key into gen->mailbox map
 * [INOUT] msg  -- location into which to store message pointer
 * [IN] flags   -- control flags
 *
 * accepted flags:
 *     OSI_TRACE_SYSCALL_OP_MAIL_CHECK_FLAG_NOWAIT  -- don't cv_wait on no msgs condition
 *
 * returns:
 *   OSI_OK on success
 *   OSI_TRACE_ERROR_MAILBOX_EMPTY when mailbox empty and OSI_TRACE_SYSCALL_OP_MAIL_CHECK_FLAG_NOWAIT asserted in flags
 *   OSI_ERROR_NOMEM when a low memory condition is detected
 *   OSI_FAIL on miscellaneous failure
 */
osi_result 
osi_trace_mail_check(osi_trace_gen_id_t gen_id,
		     osi_trace_mail_message_t ** msg,
		     osi_uint32 flags)
{
    osi_result res;
    osi_trace_mailbox_t * mbox;
    osi_trace_mail_queue_t * mq;
    int code;

    res = osi_trace_gen_rgy_mailbox_get(gen_id, &mbox);
    if (OSI_RESULT_OK_LIKELY(res)) {
	osi_mutex_Lock(&mbox->lock);
	if (!mbox->len) {
	    if (flags & OSI_TRACE_SYSCALL_OP_MAIL_CHECK_FLAG_NOWAIT) {
		osi_mutex_Unlock(&mbox->lock);
		res = OSI_TRACE_ERROR_MAILBOX_EMPTY;
		goto done;
	    }
	    do {
#if defined(OSI_IMPLEMENTS_CONDVAR_WAIT_SIG)
		code = osi_condvar_WaitSig(&mbox->cv, &mbox->lock);
		if (code) {
		    osi_mutex_Unlock(&mbox->lock);
		    res = OSI_FAIL;
		    goto done;
		}
#else /* !OSI_IMPLEMENTS_CONDVAR_WAIT_SIG */
		osi_condvar_Wait(&mbox->cv, &mbox->lock);
#endif /* !OSI_IMPLEMENTS_CONDVAR_WAIT_SIG */
	    } while (!mbox->len);
	}

	/* grab the first message */
	mq = osi_list_First(&mbox->messages, osi_trace_mail_queue_t, msg_list);
	osi_list_Remove(mq, struct osi_trace_mail_queue_t, msg_list);
	mbox->len--;

	*msg = mq->msg;
	osi_trace_mail_mq_free(mq);

	osi_mutex_Unlock(&mbox->lock);
	res = OSI_OK;

    done:
	osi_trace_gen_rgy_mailbox_put(mbox);
    }

    return res;
}

/*
 * bcast api
 */

/*
 * add a mailbox to the broadcast address
 *
 * [IN] mbox  -- pointer to mailbox
 *
 * preconditions:
 *   mbox->gen->lock held
 *
 * postconditions:
 *   ref acquired on mbox->gen
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM when a low memory condition is detected
 */
osi_result
osi_trace_mail_bcast_add(osi_trace_mailbox_t * mbox)
{
    osi_result res = OSI_OK;
    struct osi_trace_mail_mcast_node * node;

    node = (struct osi_trace_mail_mcast_node *)
	osi_mem_object_cache_alloc(osi_trace_mail_mcast_cache);
    if (osi_compiler_expect_false(node == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_trace_gen_rgy_get_nl(mbox->gen);
    node->mbox = mbox;

    osi_rwlock_WrLock(&osi_trace_mail_config.lock);
    osi_list_Append(&osi_trace_mail_config.bcast,
		    node,
		    struct osi_trace_mail_mcast_node,
		    mbox_list);
    osi_rwlock_Unlock(&osi_trace_mail_config.lock);

 error:
    return res;
}

/*
 * delete a mailbox from the broadcast address
 *
 * [IN] mbox  -- pointer to mailbox
 *
 * preconditions:
 *   caller MUST hold a ref on mbox->gen
 *   mbox->gen->lock held
 *
 * postconditions:
 *   ref on mbox->gen released
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when the mailbox is not a member of the broadcast list
 */
osi_result
osi_trace_mail_bcast_del(osi_trace_mailbox_t * mbox)
{
    osi_result res = OSI_FAIL;
    struct osi_trace_mail_mcast_node * mc, * nmc;

    osi_rwlock_WrLock(&osi_trace_mail_config.lock);
    for (osi_list_Scan(&osi_trace_mail_config.bcast,
		       mc, nmc,
		       struct osi_trace_mail_mcast_node,
		       mbox_list)) {
	if (mc->mbox == mbox) {
	    osi_list_Remove(mc,
			    struct osi_trace_mail_mcast_node,
			    mbox_list);
	    goto found;
	}
    }
    osi_rwlock_Unlock(&osi_trace_mail_config.lock);
    goto done;

 found:
    osi_rwlock_Unlock(&osi_trace_mail_config.lock);
    mc->mbox = osi_NULL;
    osi_mem_object_cache_free(osi_trace_mail_mcast_cache,
			      mc);
    osi_trace_gen_rgy_put_nl_nz(mbox->gen);
    res = OSI_OK;

 done:
    return res;
}

/*
 * mcast api
 */

/*
 * initialize a multicast address
 *
 * [IN] mcast_id  -- multicast address id
 *
 * returns:
 *   OSI_OK on success
 *   OSI_TRACE_MAIL_ERROR_BAD_MCAST_ADDR when bad mcast address is passed
 *   OSI_FAIL when mcast address was previously initialized
 */
osi_result
osi_trace_mail_mcast_init(osi_trace_gen_id_t mcast_id)
{
    osi_result res = OSI_OK;
    osi_uint32 i;

    if (osi_compiler_expect_false((mcast_id < OSI_TRACE_GEN_RGY_MCAST_MIN) ||
				  (mcast_id > OSI_TRACE_GEN_RGY_MCAST_MAX))) {
	res = OSI_TRACE_MAIL_ERROR_BAD_MCAST_ADDR;
	goto error;
    }

    i = 1 << (mcast_id - OSI_TRACE_GEN_RGY_MCAST_MIN);

    osi_rwlock_WrLock(&osi_trace_mail_config.lock);
    if (osi_compiler_expect_false(osi_trace_mail_config.mcast_usage & i)) {
	res = OSI_FAIL;
    } else {
	osi_trace_mail_config.mcast_usage |= (i);
    }
    osi_rwlock_Unlock(&osi_trace_mail_config.lock);

 error:
    return res;
}

/*
 * destroy a multicast address and associated state
 *
 * [IN] mcast_id  -- multicast address id
 *
 * returns:
 *   OSI_OK on success
 *   OSI_TRACE_MAIL_ERROR_BAD_MCAST_ADDR when bad mcast address is passed
 *   OSI_FAIL when multicast address was never initialized
 */
osi_result
osi_trace_mail_mcast_destroy(osi_trace_gen_id_t mcast_id)
{
    osi_result code = OSI_OK;
    struct osi_trace_mail_mcast_node * mc, * nmc;
    osi_uint32 i;
    osi_list_head temp_queue;

    if (osi_compiler_expect_false((mcast_id < OSI_TRACE_GEN_RGY_MCAST_MIN) ||
				  (mcast_id > OSI_TRACE_GEN_RGY_MCAST_MAX))) {
	code = OSI_TRACE_MAIL_ERROR_BAD_MCAST_ADDR;
	goto error;
    }

    i = mcast_id - OSI_TRACE_GEN_RGY_MCAST_MIN;
    osi_list_Init(&temp_queue);

    osi_rwlock_WrLock(&osi_trace_mail_config.lock);
    if (osi_compiler_expect_false(!(osi_trace_mail_config.mcast_usage & (1 << i)))) {
	code = OSI_FAIL;
    } else {
	osi_list_SpliceAppend(&temp_queue, &osi_trace_mail_config.mcast[i]);
	osi_trace_mail_config.mcast_usage &= ~(1 << i);
    }
    osi_rwlock_Unlock(&osi_trace_mail_config.lock);

    for (osi_list_Scan(&temp_queue,
		       mc, nmc,
		       struct osi_trace_mail_mcast_node,
		       mbox_list)) {
	osi_list_Remove(mc,
			struct osi_trace_mail_mcast_node,
			mbox_list);
	osi_trace_gen_rgy_put(mc->mbox->gen);
	mc->mbox = osi_NULL;
	osi_mem_object_cache_free(osi_trace_mail_mcast_cache, mc);
    }

 error:
    return code;
}

/*
 * add a mailbox to a multicast address
 *
 * [IN] mcast_id       -- multicast address id
 * [IN] mbox           -- pointer to mailbox
 * [IN] gen_lock_held  -- nonzero if mbox->gen->lock held
 *
 * postconditions:
 *   ref acquired on mbox->gen
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM when a low memory condition is detected
 *   OSI_FAIL when mcast list is not initialized
 */
osi_result
osi_trace_mail_mcast_add(osi_trace_gen_id_t mcast_id, 
			 osi_trace_mailbox_t * mbox,
			 int gen_lock_held)
{
    osi_result res = OSI_OK;
    struct osi_trace_mail_mcast_node * node;
    osi_uint32 i;

    if (osi_compiler_expect_false((mcast_id < OSI_TRACE_GEN_RGY_MCAST_MIN) ||
				  (mcast_id > OSI_TRACE_GEN_RGY_MCAST_MAX))) {
	res = OSI_TRACE_MAIL_ERROR_BAD_MCAST_ADDR;
	goto error;
    }

    i = mcast_id - OSI_TRACE_GEN_RGY_MCAST_MIN;

    node = (struct osi_trace_mail_mcast_node *)
	osi_mem_object_cache_alloc(osi_trace_mail_mcast_cache);
    if (osi_compiler_expect_false(node == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }
    if (gen_lock_held) {
	osi_trace_gen_rgy_get_nl(mbox->gen);
    } else {
	osi_trace_gen_rgy_get(mbox->gen);
    }
    node->mbox = mbox;

    osi_rwlock_WrLock(&osi_trace_mail_config.lock);
    if (osi_compiler_expect_false(!(osi_trace_mail_config.mcast_usage & (1 << i)))) {
	res = OSI_FAIL;
	goto error_sync;
    } else {
	osi_list_Append(&osi_trace_mail_config.mcast[i],
			node,
			struct osi_trace_mail_mcast_node,
			mbox_list);
    }
    osi_rwlock_Unlock(&osi_trace_mail_config.lock);

 error:
 done:
    return res;

 error_sync:
    osi_rwlock_Unlock(&osi_trace_mail_config.lock);
    osi_mem_object_cache_free(osi_trace_mail_mcast_cache, node);
    goto done;
}

/*
 * delete a mailbox from a multicast address
 *
 * [IN] mcast_id       -- multicast address id
 * [IN] mbox           -- pointer to mailbox
 * [IN] gen_lock_held  -- nonzero if mbox->gen->lock held
 *
 * preconditions:
 *   caller MUST hold ref on mbox->gen
 *
 * postconditions:
 *   ref on mbox->gen released
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when the mailbox is not associated with the multicast address,
 *            OR when the mcast address is not initialized
 */
osi_result
osi_trace_mail_mcast_del(osi_trace_gen_id_t mcast_id, 
			 osi_trace_mailbox_t * mbox,
			 int gen_lock_held)
{
    osi_result res = OSI_FAIL;
    struct osi_trace_mail_mcast_node * mc, * nmc;
    osi_uint32 i;

    if (osi_compiler_expect_false((mcast_id < OSI_TRACE_GEN_RGY_MCAST_MIN) ||
				  (mcast_id > OSI_TRACE_GEN_RGY_MCAST_MAX))) {
	res = OSI_TRACE_MAIL_ERROR_BAD_MCAST_ADDR;
	goto error;
    }

    i = mcast_id - OSI_TRACE_GEN_RGY_MCAST_MIN;

    osi_rwlock_WrLock(&osi_trace_mail_config.lock);
    if (osi_compiler_expect_true(osi_trace_mail_config.mcast_usage & (1 << i))) {
	for (osi_list_Scan(&osi_trace_mail_config.mcast[i],
			   mc, nmc,
			   struct osi_trace_mail_mcast_node,
			   mbox_list)) {
	    if (mc->mbox == mbox) {
		goto found;
	    }
	}
    }
    osi_rwlock_Unlock(&osi_trace_mail_config.lock);
    goto error;

 found:
    osi_list_Remove(mc,
		    struct osi_trace_mail_mcast_node,
		    mbox_list);
    osi_rwlock_Unlock(&osi_trace_mail_config.lock);
    mc->mbox = osi_NULL;
    osi_mem_object_cache_free(osi_trace_mail_mcast_cache, mc);
    if (gen_lock_held) {
	osi_trace_gen_rgy_put_nl_nz(mbox->gen);
    } else {
	osi_trace_gen_rgy_put_nz(mbox->gen);
    }
    res = OSI_OK;

 error:
    return res;
}

/*
 * create a mailbox tap
 *
 * [IN] tappee_id  -- gen id which will be tapped
 * [IN] tapper_id  -- gen id to which tapped messages will be forwarded
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on invalid tapper or tappee id
 */
osi_result
osi_trace_mail_tap_add(osi_trace_gen_id_t tappee_id,
		       osi_trace_gen_id_t tapper_id)
{
    osi_result res = OSI_OK;
    osi_uint32 i;
    osi_trace_mailbox_t * tappee_mbox = osi_NULL, * tapper_mbox = osi_NULL;
    struct osi_trace_mail_mcast_node * node = osi_NULL;

    if ((tapper_id == OSI_TRACE_GEN_RGY_KERNEL_ID) ||
	(tapper_id > OSI_TRACE_GEN_RGY_UCAST_MAX)) {
	res = OSI_FAIL;
	goto error;
    }

    res = osi_trace_gen_rgy_mailbox_get(tapper_id,
					&tapper_mbox);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	tapper_mbox = osi_NULL;
	goto error;
    }

    node = (struct osi_trace_mail_mcast_node *)
	osi_mem_object_cache_alloc(osi_trace_mail_mcast_cache);
    if (osi_compiler_expect_false(node == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }
    node->mbox = tapper_mbox;

    if (tappee_id == OSI_TRACE_GEN_RGY_BCAST_ID) {
	/* nothing to do */
	goto cleanup;
    } else if (tappee_id >= OSI_TRACE_GEN_RGY_MCAST_MIN) {
	res = osi_trace_mail_mcast_add(tappee_id,
				       tapper_mbox,
				       OSI_TRACE_MAIL_GEN_LOCK_NOT_HELD);
	goto cleanup;
    } else if (tappee_id == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	osi_rwlock_WrLock(&osi_trace_mail_config.lock);
	if (osi_list_IsEmpty(&osi_trace_mail_config.tap)) {
	    osi_trace_mail_tap_active = 1;
	}
	osi_list_Append(&osi_trace_mail_config.tap,
			node,
			struct osi_trace_mail_mcast_node,
			mbox_list);
	osi_rwlock_Unlock(&osi_trace_mail_config.lock);
    } else {
	res = osi_trace_gen_rgy_mailbox_get(tappee_id,
					    &tappee_mbox);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    tappee_mbox = osi_NULL;
	    goto cleanup;
	}

	osi_mutex_Lock(&tappee_mbox->lock);
	if (osi_compiler_expect_false(tappee_mbox->state != OSI_TRACE_MAILBOX_OPEN)) {
	    res = OSI_FAIL;
	    osi_mutex_Unlock(&tappee_mbox->lock);
	    goto cleanup;
	}
	if (osi_list_IsEmpty(&tappee_mbox->taps)) {
	    tappee_mbox->tap_active = 1;
	}
	osi_list_Append(&tappee_mbox->taps,
			node,
			struct osi_trace_mail_mcast_node,
			mbox_list);
	osi_mutex_Unlock(&tappee_mbox->lock);
    }

 error:
    if (tappee_mbox) {
	osi_trace_gen_rgy_mailbox_put(tappee_mbox);
    }
    if (tapper_mbox) {
	osi_trace_gen_rgy_mailbox_put(tapper_mbox);
    }
    return res;

 cleanup:
    if (node) {
	osi_mem_object_cache_free(osi_trace_mail_mcast_cache, node);
    }
    goto error;
}

/*
 * delete a mailbox tap
 *
 * [IN] tappee_id  -- gen id which will be tapped
 * [IN] tapper_id  -- gen id to which tapped messages will be forwarded
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on invalid tapper or tappee id
 */
osi_result
osi_trace_mail_tap_del(osi_trace_gen_id_t tappee_id,
		       osi_trace_gen_id_t tapper_id)
{
    osi_result res = OSI_FAIL;
    osi_uint32 i;
    osi_trace_mailbox_t * tappee_mbox = osi_NULL, * tapper_mbox = osi_NULL;
    struct osi_trace_mail_mcast_node * node, * nn;

    if ((tapper_id == OSI_TRACE_GEN_RGY_KERNEL_ID) ||
	(tapper_id > OSI_TRACE_GEN_RGY_UCAST_MAX)) {
	res = OSI_FAIL;
	goto error;
    }

    res = osi_trace_gen_rgy_mailbox_get(tapper_id,
					&tapper_mbox);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	tapper_mbox = osi_NULL;
	goto error;
    }

    if (tappee_id == OSI_TRACE_GEN_RGY_BCAST_ID) {
	/* nothing to do */
	res = OSI_OK;
	goto done;
    } else if (tappee_id >= OSI_TRACE_GEN_RGY_MCAST_MIN) {
	res = osi_trace_mail_mcast_del(tappee_id,
				       tapper_mbox,
				       OSI_TRACE_MAIL_GEN_LOCK_NOT_HELD);
	goto done;
    } else if (tappee_id == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	osi_rwlock_WrLock(&osi_trace_mail_config.lock);
	for (osi_list_Scan(&osi_trace_mail_config.tap,
			   node, nn,
			   struct osi_trace_mail_mcast_node,
			   mbox_list)) {
	    if (node->mbox == tapper_mbox) {
		res = OSI_OK;
		osi_list_Remove(node,
				struct osi_trace_mail_mcast_node,
				mbox_list);
		break;
	    }
	}
	if (osi_list_IsEmpty(&osi_trace_mail_config.tap)) {
	    osi_trace_mail_tap_active = 0;
	}
	osi_rwlock_Unlock(&osi_trace_mail_config.lock);
    } else {
	res = osi_trace_gen_rgy_mailbox_get(tappee_id,
					    &tappee_mbox);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    tappee_mbox = osi_NULL;
	    goto error;
	}

	osi_mutex_Lock(&tappee_mbox->lock);
	for (osi_list_Scan(&tappee_mbox->taps,
			   node, nn,
			   struct osi_trace_mail_mcast_node,
			   mbox_list)) {
	    if (node->mbox == tapper_mbox) {
		res = OSI_OK;
		osi_list_Remove(node,
				struct osi_trace_mail_mcast_node,
				mbox_list);
		break;
	    }
	}
	if (osi_list_IsEmpty(&tappee_mbox->taps) &&
	    tappee_mbox->tap_active) {
	    tappee_mbox->tap_active = 0;
	}
	osi_mutex_Unlock(&tappee_mbox->lock);
    }

    if (OSI_RESULT_OK_LIKELY(res)) {
	node->mbox = osi_NULL;
	osi_mem_object_cache_free(osi_trace_mail_mcast_cache, node);
    }

 done:
 error:
    if (tappee_mbox) {
	osi_trace_gen_rgy_mailbox_put(tappee_mbox);
    }
    if (tapper_mbox) {
	osi_trace_gen_rgy_mailbox_put(tapper_mbox);
    }
    return res;
}


/*
 * syscall interface
 */

/*
 * send a message syscall
 *
 * [IN] arg  -- pointer to osi_trace_mail_message_t in userspace context
 *
 * possible errno values:
 *   EAGAIN on memory allocation failure
 *   EFAULT on fault in userspace copyin/copyout
 */
int
osi_trace_mail_sys_send(void * arg)
{
    int code = 0;
    osi_result res = OSI_OK;
    osi_trace_mail_message_t * kmsg = osi_NULL;
    osi_trace_mail_message_t * umsg = osi_NULL;

    umsg = (osi_trace_mail_message_t *)
	osi_mem_alloc(sizeof(osi_trace_mail_message_t));
    if (osi_compiler_expect_false(umsg == osi_NULL)) {
	code = EAGAIN;
	goto cleanup;
    }
    osi_kernel_handle_copy_in(arg, umsg, sizeof(*umsg), code, cleanup);
    
    res = osi_trace_mail_msg_alloc(umsg->body_len, &kmsg);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EAGAIN;
	goto cleanup;
    }
    
    osi_mem_copy(&kmsg->envelope, &umsg->envelope, sizeof(kmsg->envelope));
    
    osi_kernel_handle_copy_in(umsg->body, kmsg->body, kmsg->body_len, code, cleanup);

    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_mail_send(kmsg))) {
	code = EAGAIN;
	goto cleanup;
    }

 cleanup:
    if (osi_compiler_expect_true(umsg != osi_NULL)) {
	osi_mem_free(umsg, sizeof(*umsg));
    }
    if (osi_compiler_expect_true(kmsg != osi_NULL)) {
	osi_trace_mail_msg_put(kmsg);
    }

    return code;
}

/*
 * send a vector of messages syscall
 *
 * [IN] arg  -- pointer to osi_trace_mail_mvec_t in userspace context
 *
 * possible errno values:
 *   EAGAIN on memory allocation failure
 *   EFAULT on fault in userspace copyin/copyout
 */
int
osi_trace_mail_sys_sendv(void * arg)
{
    int code = 0;
    osi_uint32 i;
    osi_trace_mail_mvec_t * umv;

    umv = (osi_trace_mail_mvec_t *)
	osi_mem_alloc(sizeof(osi_trace_mail_mvec_t));
    if (osi_compiler_expect_false(umv == osi_NULL)) {
	code = EAGAIN;
	goto cleanup;
    }

    osi_kernel_handle_copy_in(arg, umv, sizeof(*umv), code, cleanup);

    for (i = 0; i < umv->nrec; i++) {
	code = osi_trace_mail_sys_send((void *)umv->mvec[i]);
	if (osi_compiler_expect_false(code)) {
	    goto cleanup;
	}
    }

 cleanup:
    if (osi_compiler_expect_true(umv != osi_NULL)) {
	osi_mem_free(umv, sizeof(*umv));
    }
    return code;
}

/*
 * receive a message syscall
 *
 * [IN] arg    -- pointer to osi_trace_mail_message_t in userspace context
 * [IN] flags  -- control flags
 *
 * accepted flags:
 *     OSI_TRACE_SYSCALL_OP_MAIL_CHECK_FLAG_NOWAIT  -- don't cv_wait on no msgs condition
 *
 * possible errno values:
 *   EAGAIN on memory allocation failure
 *   EFAULT on fault in userspace copyin/copyout
 */
int
osi_trace_mail_sys_check(osi_trace_gen_id_t gen_id,
			 void * arg,
			 osi_uint32 flags)
{
    int code = 0;
    osi_result res;
    osi_trace_mail_message_t * kmsg = osi_NULL;
    osi_trace_mail_message_t * umsg = osi_NULL;

    umsg = (osi_trace_mail_message_t *)
	osi_mem_alloc(sizeof(osi_trace_mail_message_t));
    if (osi_compiler_expect_false(umsg == osi_NULL)) {
	code = EAGAIN;
	goto cleanup;
    }

    osi_kernel_handle_copy_in(arg, umsg, sizeof(*umsg), code, cleanup);

    res = osi_trace_mail_check(gen_id, &kmsg, flags);
    if (OSI_RESULT_FAIL(res)) {
	if (res == OSI_TRACE_ERROR_MAILBOX_EMPTY) {
	    code = EWOULDBLOCK;
	} else {
	    code = EAGAIN;
	}
	goto cleanup;
    }

    if (kmsg->body_len > umsg->body_len) {
	code = EINVAL;
	goto cleanup;
    }

    osi_kernel_handle_copy_out(kmsg->body, umsg->body, kmsg->body_len, code, cleanup);
    osi_kernel_handle_copy_out(&kmsg->envelope, arg, sizeof(kmsg->envelope), code, cleanup);

 cleanup:
    if (osi_compiler_expect_true(umsg != osi_NULL)) {
	osi_mem_free(umsg, sizeof(*umsg));
    }
    if (osi_compiler_expect_true(kmsg != osi_NULL)) {
	osi_trace_mail_msg_put(kmsg);
    }
    return code;
}

int
osi_trace_mail_sys_checkv(osi_trace_gen_id_t gen_id,
			  void * arg,
			  osi_uint32 flags,
			  long * retVal)
{
    int code = 0;
    osi_uint32 i;
    osi_trace_mail_mvec_t * umv;

    umv = (osi_trace_mail_mvec_t *)
	osi_mem_alloc(sizeof(osi_trace_mail_mvec_t));
    if (osi_compiler_expect_false(umv == osi_NULL)) {
	code = EAGAIN;
	goto cleanup;
    }

    osi_kernel_handle_copy_in(arg, umv, sizeof(*umv), code, cleanup);

    for (i = 0; i < umv->nrec; i++) {
	if (i==1) {
	    flags |= OSI_TRACE_SYSCALL_OP_MAIL_CHECK_FLAG_NOWAIT;
	}
	code = osi_trace_mail_sys_check(gen_id,
					(void *)umv->mvec[i],
					flags);
	if (code == EWOULDBLOCK) {
	    code = 0;
	    break;
	} else if (osi_compiler_expect_false(code)) {
	    goto cleanup;
	}
    }

    *retVal = (long) i;

 cleanup:
    if (osi_compiler_expect_true(umv != osi_NULL)) {
	osi_mem_free(umv, sizeof(*umv));
    }
    return code;
}

int
osi_trace_mail_sys_tap(long tappee_id,
		       long tapper_id)
{
    int code = 0;
    osi_result res;

    res = osi_trace_mail_tap_add(tappee_id, tapper_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	if (res == OSI_ERROR_NOMEM) {
	    code = EAGAIN;
	} else {
	    code = EINVAL;
	}
    }

    return code;
}

int
osi_trace_mail_sys_untap(long tappee_id,
			 long tapper_id)
{
    int code = 0;
    osi_result res;

    res = osi_trace_mail_tap_del(tappee_id, tapper_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EINVAL;
    }

    return code;
}

#if (!OSI_DATAMODEL_IS(OSI_ILP32_ENV))
/*
 * syscall32 interface
 */

int
osi_trace_mail_sys32_send(void * arg)
{
    int code = 0;
    osi_result res = OSI_OK;
    osi_trace_mail_message_t * kmsg = osi_NULL;
    osi_trace_mail_message32_t * umsg = osi_NULL;

    umsg = (osi_trace_mail_message32_t *)
	osi_mem_alloc(sizeof(osi_trace_mail_message32_t));
    if (osi_compiler_expect_false(umsg == osi_NULL)) {
	code = EAGAIN;
	goto cleanup;
    }

    osi_kernel_handle_copy_in(arg, umsg, sizeof(*umsg), code, cleanup);

    res = osi_trace_mail_msg_alloc(umsg->body_len, &kmsg);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = EAGAIN;
	goto cleanup;
    }

    osi_mem_copy(&kmsg->envelope, &umsg->envelope, sizeof(kmsg->envelope));

    osi_kernel_handle_copy_in(umsg->body, kmsg->body, kmsg->body_len, code, cleanup);

    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_mail_send(kmsg))) {
	code = EAGAIN;
    }

 cleanup:
    if (osi_compiler_expect_true(umsg != osi_NULL)) {
	osi_mem_free(umsg, sizeof(*umsg));
    }
    if (osi_compiler_expect_false(kmsg != osi_NULL)) {
	osi_trace_mail_msg_put(kmsg);
    }

    return code;
}

int
osi_trace_mail_sys32_sendv(void * arg)
{
    int code = 0;
    osi_uint32 i;
    osi_trace_mail_mvec32_t * umv;

    umv = (osi_trace_mail_mvec32_t *)
	osi_mem_alloc(sizeof(osi_trace_mail_mvec32_t));
    if (osi_compiler_expect_false(umv == osi_NULL)) {
	code = EAGAIN;
	goto cleanup;
    }

    osi_kernel_handle_copy_in(arg, umv, sizeof(*umv), code, cleanup);

    for (i = 0; i < umv->nrec; i++) {
	code = osi_trace_mail_sys32_send((void *)umv->mvec[i]);
	if (osi_compiler_expect_false(code)) {
	    goto cleanup;
	}
    }

 cleanup:
    if (osi_compiler_expect_true(umv != osi_NULL)) {
	osi_mem_free(umv, sizeof(*umv));
    }
    return code;
}

int
osi_trace_mail_sys32_check(osi_trace_gen_id_t gen_id,
			   void * arg,
			   osi_uint32 flags)
{
    int code = 0;
    osi_result res;
    osi_trace_mail_message_t * kmsg = osi_NULL;
    osi_trace_mail_message32_t * umsg = osi_NULL;

    umsg = (osi_trace_mail_message32_t *)
	osi_mem_alloc(sizeof(osi_trace_mail_message32_t));
    if (osi_compiler_expect_false(umsg == osi_NULL)) {
	code = EAGAIN;
	goto cleanup;
    }

    osi_kernel_handle_copy_in(arg, umsg, sizeof(*umsg), code, cleanup);

    res = osi_trace_mail_check(gen_id, &kmsg, flags);
    if (OSI_RESULT_FAIL(res)) {
	if (res == OSI_TRACE_ERROR_MAILBOX_EMPTY) {
	    code = EWOULDBLOCK;
	} else {
	    code = EAGAIN;
	}
	goto cleanup;
    }

    if (kmsg->body_len > umsg->body_len) {
	code = EINVAL;
	goto cleanup;
    }

    osi_kernel_handle_copy_out(kmsg->body, umsg->body, kmsg->body_len, code, cleanup);
    osi_kernel_handle_copy_out(&kmsg->envelope, arg, sizeof(kmsg->envelope), code, cleanup);

 cleanup:
    if (osi_compiler_expect_true(umsg != osi_NULL)) {
	osi_mem_free(umsg, sizeof(*umsg));
    }
    if (osi_compiler_expect_true(kmsg != osi_NULL)) {
	osi_trace_mail_msg_put(kmsg);
    }
    return code;
}

int
osi_trace_mail_sys32_checkv(osi_trace_gen_id_t gen_id,
			    void * arg,
			    osi_uint32 flags,
			    long * retVal)
{
    int code = 0;
    osi_uint32 i;
    osi_trace_mail_mvec32_t * umv;

    umv = (osi_trace_mail_mvec32_t *)
	osi_mem_alloc(sizeof(osi_trace_mail_mvec32_t));
    if (osi_compiler_expect_false(umv == osi_NULL)) {
	code = EAGAIN;
	goto cleanup;
    }

    osi_kernel_handle_copy_in(arg, umv, sizeof(*umv), code, cleanup);

    for (i = 0; i < umv->nrec; i++) {
	if (i==1) {
	    flags |= OSI_TRACE_SYSCALL_OP_MAIL_CHECK_FLAG_NOWAIT;
	}
	code = osi_trace_mail_sys32_check(gen_id,
					  (void *)umv->mvec[i],
					  flags);
	if (code == EWOULDBLOCK) {
	    code = 0;
	    break;
	} else if (osi_compiler_expect_false(code)) {
	    goto cleanup;
	}
    }

    *retVal = (long) i;

 cleanup:
    if (osi_compiler_expect_true(umv != osi_NULL)) {
	osi_mem_free(umv, sizeof(*umv));
    }
    return code;
}
#endif /* !OSI_DATAMODEL_IS(OSI_ILP32_ENV) */


osi_result
osi_trace_mail_postmaster_PkgInit(void)
{
    osi_result res = OSI_OK;
    int i;

    osi_rwlock_Init(&osi_trace_mail_config.lock,
		    osi_trace_impl_rwlock_opts());

    osi_trace_mail_config.mcast_usage = 0;

    osi_list_Init(&osi_trace_mail_config.bcast);
    osi_list_Init(&osi_trace_mail_config.tap);
    for (i = 0; i < OSI_TRACE_MAIL_MCAST_ID_COUNT; i++) {
	osi_list_Init(&osi_trace_mail_config.mcast[i]);
    }

    osi_trace_mail_mcast_cache =
	osi_mem_object_cache_create("afs_osi_trace_mail_mcast_cache",
				    sizeof(struct osi_trace_mail_mcast_node),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_trace_mail_mcast_cache == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    res = osi_trace_mail_mcast_init(OSI_TRACE_GEN_RGY_MCAST_CONSUMER);
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_mail_mcast_init(OSI_TRACE_GEN_RGY_MCAST_GENERATOR);

 error:
    return res;
}

osi_result
osi_trace_mail_postmaster_PkgShutdown(void)
{
    osi_result res;
    struct osi_trace_mail_mcast_node *mc, *nmc;
    int i;

    res = osi_trace_mail_mcast_destroy(OSI_TRACE_GEN_RGY_MCAST_GENERATOR);
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_mail_mcast_destroy(OSI_TRACE_GEN_RGY_MCAST_CONSUMER);
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    osi_rwlock_WrLock(&osi_trace_mail_config.lock);

    /* destroy the broadcast address mailbox list */
    for (osi_list_Scan(&osi_trace_mail_config.bcast,
		       mc, nmc,
		       struct osi_trace_mail_mcast_node,
		       mbox_list)) {
	osi_list_Remove(mc, struct osi_trace_mail_mcast_node, mbox_list);
	osi_mem_object_cache_free(osi_trace_mail_mcast_cache, mc);
    }

    /* destroy the multicast address mailbox lists */
    for (i = 0; i < OSI_TRACE_MAIL_MCAST_ID_COUNT; i++) {
	for (osi_list_Scan(&osi_trace_mail_config.mcast[i],
			   mc, nmc,
			   struct osi_trace_mail_mcast_node,
			   mbox_list)) {
	    osi_list_Remove(mc, struct osi_trace_mail_mcast_node, mbox_list);
	    osi_mem_object_cache_free(osi_trace_mail_mcast_cache, mc);
	}
    }

    osi_rwlock_Unlock(&osi_trace_mail_config.lock);
    osi_rwlock_Destroy(&osi_trace_mail_config.lock);

    osi_mem_object_cache_destroy(osi_trace_mail_mcast_cache);

 error:
    return res;
}
