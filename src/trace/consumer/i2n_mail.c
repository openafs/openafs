/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_string.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/consumer/i2n_mail.h>
#include <trace/consumer/cache/generator.h>
#include <trace/consumer/cache/binary.h>
#include <trace/consumer/cache/probe_info.h>

/*
 * osi tracing framework
 * trace consumer library
 * i2n cache coherency messaging
 */

/* static prototypes */
osi_static osi_result
osi_trace_consumer_i2n_msg_res(osi_trace_mail_message_t * msg);



osi_static osi_result
osi_trace_consumer_i2n_msg_res(osi_trace_mail_message_t * msg)
{
    osi_result res = OSI_OK;
    osi_trace_mail_msg_probe_i2n_response_t * payload;
    size_t probe_name_len;
    osi_trace_consumer_probe_info_cache_t * probe = osi_NULL;
    osi_trace_consumer_bin_cache_t * bin;

    payload = (osi_trace_mail_msg_probe_i2n_response_t *) msg->body;
    payload->probe_name[sizeof(payload->probe_name)-1] = '\0';
    probe_name_len = osi_string_len(payload->probe_name);

    /* get a handle to the bin */
    res = osi_trace_consumer_gen_cache_lookup_bin(msg->envelope.env_src,
						  &bin);
    if (OSI_RESULT_FAIL(res)) {
	bin = osi_NULL;
	goto error;
    }

    /* look for an existing probe */
    res = osi_trace_consumer_bin_cache_lookup_probe(bin,
						    payload->probe_id,
						    &probe);

    /* otherwise, allocate a new probe */
    if (OSI_RESULT_FAIL(res)) {
	res = osi_trace_consumer_probe_info_cache_alloc(payload->probe_name,
							probe_name_len,
							&probe);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    probe = osi_NULL;
	    goto cleanup;
	}
    }

    /* populate the probe object */
    res = osi_trace_consumer_probe_info_cache_populate(probe,
						       payload);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto cleanup;
    }

 error:
    return res;

 cleanup:
    /* ok, we failed. let's clean up dangling refs */
    if (probe) {
	osi_trace_consumer_probe_info_cache_put(probe);
    }
    if (bin) {
	osi_trace_consumer_bin_cache_put(bin);
    }
}

osi_result
osi_trace_consumer_i2n_mail_PkgInit(void)
{
    osi_result res;

    /* 
     * register the various mail handlers 
     */

    /* incoming i2n response messages are used to populate the cache */
    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_DIRECTORY_I2N_RES,
					      &osi_trace_consumer_i2n_msg_res);

 error:
    return res;
}

osi_result
osi_trace_consumer_i2n_mail_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_DIRECTORY_I2N_RES,
						&osi_trace_consumer_i2n_msg_res);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

 error:
    return res;
}
