/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi tracing framework
 * inter-process asynchronous messaging
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_mem.h>
#include <osi/osi_cache.h>
#include <osi/osi_object_cache.h>
#include <trace/mail.h>
#include <trace/gen_rgy.h>
#include <trace/common/options.h>

osi_mem_object_cache_t * osi_trace_mail_msg_cache = osi_NULL;
osi_mem_object_cache_t * osi_trace_mail_mq_cache = osi_NULL;

struct osi_trace_mail_msg_cache_blob {
    osi_trace_mail_message_t msg;
    osi_trace_mail_message_local_data_t ldata;
};

/* static prototypes */
osi_static int osi_trace_mail_msg_cache_ctor(void * buf, void * sdata, int flags);
osi_static void osi_trace_mail_msg_cache_dtor(void * buf, void * sdata);


osi_static int
osi_trace_mail_msg_cache_ctor(void * buf, void * sdata, int flags)
{
    struct osi_trace_mail_msg_cache_blob * blob = buf;


    blob->msg.ldata = &blob->ldata;
    osi_refcnt_init(&blob->ldata.refcnt, 0);
    return 0;
}

osi_static void
osi_trace_mail_msg_cache_dtor(void * buf, void * sdata)
{
    struct osi_trace_mail_msg_cache_blob * blob = buf;


    osi_refcnt_destroy(&blob->ldata.refcnt);
}

/*
 * allocate a message buffer
 * set reference count to 1
 *
 * [IN] body_len  -- length of buffer to allocate for the message body
 * [OUT] msg      -- pointer to location in which to store new message pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_mail_msg_alloc(osi_uint32 body_len, osi_trace_mail_message_t ** msg_out)
{
    osi_result res = OSI_FAIL;
    osi_trace_mail_message_t * msg;

    *msg_out = msg = (osi_trace_mail_message_t *)
	osi_mem_object_cache_alloc(osi_trace_mail_msg_cache);
    if (osi_compiler_expect_false(msg == osi_NULL)) {
	goto error;
    }

    if (body_len) {
	msg->body = osi_mem_alloc(body_len);
	if (osi_compiler_expect_false(msg->body == osi_NULL)) {
	    goto error;
	}
	msg->body_len = body_len;
    } else {
	msg->body = osi_NULL;
	msg->body_len = 0;
    }
    osi_refcnt_inc(&msg->ldata->refcnt);
    res = OSI_OK;

 done:
    return res;

 error:
    if (msg != osi_NULL) {
	if (msg->body != osi_NULL) {
	    osi_mem_free(msg->body, body_len);
	}
	osi_mem_object_cache_free(osi_trace_mail_msg_cache, msg);
    }
    goto done;
}

/*
 * deallocate a message object
 *
 * [IN] arg  -- opaque pointer to a message object
 *
 * returns:
 *   OSI_OK always
 */
osi_result
__osi_trace_mail_msg_really_free(void * arg)
{
    osi_trace_mail_message_t * msg =
	(osi_trace_mail_message_t *) arg;

    if (msg->body) {
	osi_mem_free(msg->body, msg->body_len);
	msg->body = osi_NULL;
	msg->body_len = 0;
    }
    osi_mem_object_cache_free(osi_trace_mail_msg_cache, msg);
    return OSI_OK;
}

/*
 * increment reference count on a message buffer
 *
 * [IN] msg -- pointer to message
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_mail_msg_get(osi_trace_mail_message_t * msg)
{
    osi_refcnt_inc(&msg->ldata->refcnt);
    return OSI_OK;
}

/*
 * decrement reference count on a message buffer
 *
 * postconditions:
 *   deallocate if reference count reaches zero
 *
 * [IN] msg -- pointer to message
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_mail_msg_put(osi_trace_mail_message_t * msg)
{
    osi_result res = OSI_OK;
    (void)osi_refcnt_dec_action(&msg->ldata->refcnt,
				0,
				&__osi_trace_mail_msg_really_free,
				(void *)msg,
				&res);
    return res;
}

/*
 * allocate a message queue element
 *
 * [INOUT] mq_out -- address in which to store queue pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_mail_mq_alloc(osi_trace_mail_queue_t ** mq_out)
{
    osi_result res = OSI_OK;
    osi_trace_mail_queue_t * mq;

    *mq_out = mq = (osi_trace_mail_queue_t *)
	osi_mem_object_cache_alloc(osi_trace_mail_mq_cache);
    if (osi_compiler_expect_false(mq == osi_NULL)) {
	res = OSI_FAIL;
    }

    return res;
}

/*
 * free a message queue element
 *
 * [IN] mq -- pointer to message queue element to deallocate
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_mail_mq_free(osi_trace_mail_queue_t * mq)
{
    osi_mem_object_cache_free(osi_trace_mail_mq_cache, mq);
    return OSI_OK;
}

osi_result
osi_trace_mail_allocator_PkgInit(void)
{
    osi_result res = OSI_OK;
    size_t align;

    if (OSI_RESULT_FAIL_UNLIKELY(osi_cache_max_alignment(&align))) {
	align = 64;
    }

    osi_trace_mail_msg_cache =
	osi_mem_object_cache_create("osi_trace_mail_msg_cache",
				    sizeof(struct osi_trace_mail_msg_cache_blob),
				    align,
				    osi_NULL,
				    &osi_trace_mail_msg_cache_ctor,
				    &osi_trace_mail_msg_cache_dtor,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_compiler_expect_false(osi_trace_mail_msg_cache == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    osi_trace_mail_mq_cache =
	osi_mem_object_cache_create("osi_trace_mail_mq_cache",
				    sizeof(osi_trace_mail_queue_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_compiler_expect_false(osi_trace_mail_mq_cache == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

 error:
    return res;
}

osi_result
osi_trace_mail_allocator_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    osi_mem_object_cache_destroy(osi_trace_mail_msg_cache);
    osi_mem_object_cache_destroy(osi_trace_mail_mq_cache);

    return res;
}
