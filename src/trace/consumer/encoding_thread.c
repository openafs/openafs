/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_thread.h>
#include <osi/osi_mem.h>
#include <osi/osi_condvar.h>
#include <osi/osi_mutex.h>
#include <osi/osi_object_cache.h>
#include <trace/directory.h>
#include <trace/gen_rgy.h>
#include <trace/consumer/i2n.h>
#include <trace/consumer/encoding_thread.h>
#include <trace/consumer/record_queue.h>
#include <trace/consumer/record_queue_impl.h>
#include <trace/encoding/handle.h>
#include <trace/agent/trap.h>


/*
 * osi tracing framework
 * trace consumer library
 * trap encoding work queue
 * thread pool
 */

osi_trace_consumer_record_queue_t *
osi_trace_consumer_encoding_record_queue = osi_NULL;


/*
 * encoding worker thread
 */
OSI_THREAD_FUNC_DECL(osi_trace_consumer_encoding_handler_thread)
{
    osi_result res;
    osi_TracePoint_record_queue_t * rq;
    osi_trace_trap_enc_handle_t * handle;
    osi_TracePoint_record_ptr_t rec_ptr;
    void * buf;
    osi_size_t buf_len;

    while (1) {
	handle = osi_NULL;

	res = osi_trace_consumer_record_queue_dequeue(osi_trace_consumer_incoming_record_queue,
						      &rq);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    if (res != OSI_ERROR_CANCEL_WAITERS) {
		(osi_Msg "unexpected fatal error code (%d) from osi_trace_consumer_record_queue_dequeue()\n", res);
	    }
	    goto error;
	}

	res = osi_trace_trap_enc_handle_alloc(OSI_TRACE_TRAP_ENC_DIRECTION_ENCODE,
					      OSI_TRACE_TRAP_ENCODING_ASCII,
					      &handle);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    (osi_Msg "encoding thread encountered out of memory condition\n");
	    goto error;
	}

	/* XXX record versioning */
	rec_ptr.version = OSI_TRACEPOINT_RECORD_VERSION;
	rec_ptr.ptr.cur = &rq->record;

	res = osi_trace_trap_enc_handle_setup_encode(handle,
						     &rec_ptr);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto fail;
	}

	res = osi_trace_trap_enc_handle_encode(handle);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto fail;
	}

	res = osi_trace_trap_enc_handle_get_encoding(handle,
						     &buf,
						     &buf_len);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto fail;
	}

	res = osi_trace_console_trap(rq->record.gen_id,
				     rq->record.probe,
				     OSI_TRACE_TRAP_ENCODING_ASCII,
				     buf,
				     buf_len);
    fail:
	if (handle != osi_NULL) {
	    osi_trace_trap_enc_handle_put(handle);
	}
	/* XXX transition record state */
    }

 error:
    return osi_NULL;
}


/*
 * start encoding background worker threads
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on thread create failure
 */
osi_static osi_result
osi_trace_consumer_encoding_thread_start(void)
{
    osi_result res = OSI_OK;
    osi_thread_p t;
    osi_thread_options_t opt;

    osi_thread_options_Copy(&opt,
			    osi_trace_impl_thread_opts());
    osi_thread_options_Set(&opt, OSI_THREAD_OPTION_DETACHED, 1);

    res = osi_thread_createU(&t,
			     &osi_trace_consumer_encoding_handler_thread,
			     osi_NULL,
			     &opt);

 error:
    osi_thread_options_Destroy(&opt);
    return res;
}

/*
 * shutdown encoding background worker threads
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on thread shutdown failure
 */
osi_static osi_result
osi_trace_consumer_encoding_thread_shutdown(void)
{
    osi_result res = OSI_OK;

    /*
     * forcibly wakeup the handler thread
     */
    osi_trace_consumer_record_queue_waiter_cancel(osi_trace_consumer_encoding_record_queue);

    /* XXX thread join */

 error:
    return res;
}


OSI_INIT_FUNC_DECL(osi_trace_consumer_encoding_thread_PkgInit)
{
    osi_result res, code = OSI_OK;
    osi_options_val_t opt;

    res = osi_config_options_Get(OSI_OPTION_TRACE_CONSUMER_START_ENCODING_THREAD,
				 &opt);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "failed to get TRACE_CONSUMER_START_ENCODING_THREAD option value\n");
	goto error;
    }

    res = osi_trace_consumer_record_queue_create(&osi_trace_consumer_encoding_record_queue);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    if (opt.val.v_bool == OSI_TRUE) {
	code = osi_trace_consumer_encoding_thread_start();
    }

 error:
    return code;
}

OSI_FINI_FUNC_DECL(osi_trace_consumer_encoding_thread_PkgShutdown)
{
    osi_result res, code = OSI_OK;

    code = osi_trace_consumer_encoding_thread_shutdown();

    res = osi_trace_consumer_record_queue_destroy(osi_trace_consumer_encoding_record_queue);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    return code;
}
