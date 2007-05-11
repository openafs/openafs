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
#include <osi/osi_cpu.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_condvar.h>
#include <trace/USERSPACE/mail.h>
#include <trace/USERSPACE/gen_rgy.h>
#include <trace/syscall.h>

/*
 * osi tracing framework
 * inter-process asynchronous messaging
 * LWP receive thread
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

    /* XXX 
     * for now we'll implement with the non-vectorized mail check interface
     * if profiling reveals a bottleneck we'll come back and recode to use
     * the vectorized receive interface
     */
    while (1) {
	msg = osi_NULL;
	res = osi_trace_mail_msg_alloc(OSI_TRACE_MAIL_BODY_LEN_MAX, &msg);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    /* throttle on error */
	    IOMGR_Sleep(1);
	    continue;
	}

	res = osi_trace_mail_check(my_id, msg,
				   OSI_TRACE_SYSCALL_OP_MAIL_CHECK_FLAG_NOWAIT);
        if ((res != OSI_TRACE_MAIL_RESULT_NO_MAIL) && OSI_RESULT_OK_LIKELY(res)) {
	    osi_trace_mail_msg_recv_handler(msg);
	} else {
	    (void)osi_trace_mail_msg_put(msg);

	    /* if there's no mail, or there was an error checking mail,
	     * throttle loop speed */
	    IOMGR_Sleep(1);
	}
    }

 error:
    return osi_NULL;
}

