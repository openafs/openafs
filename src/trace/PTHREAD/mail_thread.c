/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_thread.h>
#include <osi/osi_cpu.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_condvar.h>
#include <trace/USERSPACE/mail.h>
#include <trace/USERSPACE/gen_rgy.h>

/*
 * osi tracing framework
 * trace consumer thread
 */


void *
osi_trace_mail_receiver(void * arg_in)
{
    osi_result res;
    osi_trace_mail_message_t * msg;
    osi_trace_gen_id_t my_id;

    if (OSI_RESULT_FAIL_UNLIKELY(osi_trace_gen_id(&my_id))) {
	goto error;
    }

    while (1) {
	msg = osi_NULL;
	res = osi_trace_mail_msg_alloc(OSI_TRACE_MAIL_BODY_LEN_MAX, &msg);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    /* throttle on errors */
	    sleep(1);
	    continue;
	}

	res = osi_trace_mail_check(my_id, msg, 0);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    /* throttle on errors */
	    sleep(1);
	    continue;
	}

	osi_trace_mail_msg_recv_handler(msg);
	osi_trace_mail_msg_put(msg);
    }

 error:
    return osi_NULL;
}

