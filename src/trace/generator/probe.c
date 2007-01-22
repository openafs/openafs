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
 * trace data generator library
 * probe point data registry
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <trace/generator/activation.h>
#include <trace/generator/probe_impl.h>
#include <trace/generator/probe_mail.h>
#include <trace/generator/probe.h>
#include <trace/generator/directory.h>
#include <trace/generator/directory_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_string.h>


osi_result
osi_trace_probe_id(osi_trace_probe_t probe, osi_trace_probe_id_t * probe_id)
{
    *probe_id = probe->probe_id;
    return OSI_OK;
}

#if defined(OSI_USERSPACE_ENV)
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/mail/handler.h>
#include <trace/USERSPACE/mail.h>

/* XXX this code is not ready for use yet */
osi_static osi_result
osi_trace_probe_register_notify(osi_trace_probe_t probe)
{
    osi_result code;
    osi_trace_mail_message_t * msg;
    osi_trace_mail_msg_probe_register_t * payload;

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_probe_register_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    payload = (osi_trace_mail_msg_probe_register_t *) msg->body;
    payload->probe_id = probe->probe_id;
    /* XXX the following field needs to come out of the directory */
#if 0
    osi_string_ncpy(payload->probe_name, probe->probe_name, sizeof(payload->probe_name));
#else
    osi_mem_zero(payload->probe_name, sizeof(payload->probe_name));
#endif

    code = osi_trace_mail_prepare_send(msg,
				       OSI_TRACE_GEN_RGY_MCAST_CONSUMER,
				       0,
				       0,
				       OSI_TRACE_MAIL_MSG_PROBE_REGISTER);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_send(msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

 error:
    if (osi_compiler_expect_true(msg != osi_NULL)) {
	osi_trace_mail_msg_put(msg);
    }
    return code;
}
#endif /* OSI_USERSPACE_ENV */

osi_result
osi_trace_probe_register(osi_trace_probe_t * probe_out,
			 osi_trace_probe_id_t * probe_id_out,
			 int mode)
{
    osi_result res = OSI_OK;
    struct osi_trace_probe * probe;
    osi_mutex_options_t opt;

    probe = (struct osi_trace_probe *)
	osi_mem_alloc(sizeof(struct osi_trace_probe));
    if (osi_compiler_expect_false(probe == osi_NULL)) {
	res = OSI_FAIL;
	goto done;
    }

    osi_mem_zero(probe, sizeof(struct osi_trace_probe));

    res = osi_trace_directory_probe_id_alloc(&probe->probe_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	osi_mem_free(probe, sizeof(struct osi_trace_probe));
	goto done;
    }

    opt.preemptive_only = 1;
    opt.trace_enabled = 0;
    opt.trace_allowed = 0;
    osi_mutex_Init(&probe->probe_lock, &opt);

    *probe_out = probe;
    *probe_id_out = probe->probe_id;

    if (mode) {
	/* XXX need to decide whether it is better to just
	 * turn on the activation bit in the bit vector, or
	 * to actually bump the refcount so a default-enabled
	 * probe can never be disabled 
	 *
	 * for now let's just enable by default and NOT bump
	 * the refcount
	 */
	osi_TracePoint_Activate(probe->probe_id);
    }

 done:
    return res;
}

osi_result
osi_trace_probe_enable(osi_trace_probe_t probe)
{
    osi_result res = OSI_OK;

    osi_mutex_Lock(&probe->probe_lock);
    if (++probe->activation_count == 1) {
	res = osi_TracePoint_Activate(probe->probe_id);
    }
    osi_mutex_Unlock(&probe->probe_lock);

    return res;
}

osi_result
osi_trace_probe_disable(osi_trace_probe_t probe)
{
    osi_result res = OSI_OK;

    osi_mutex_Lock(&probe->probe_lock);
    if (--probe->activation_count == 0) {
	res = osi_TracePoint_Deactivate(probe->probe_id);
    }
    osi_mutex_Unlock(&probe->probe_lock);

    return res;
}

osi_result
osi_trace_probe_enable_by_id(osi_trace_probe_id_t id)
{
    osi_result res;
    osi_trace_probe_t probe;

    res = osi_trace_directory_I2P(id, &probe);
    if (OSI_RESULT_OK_LIKELY(res)) {
	res = osi_trace_probe_enable(probe);
    }

    return res;
}

osi_result
osi_trace_probe_disable_by_id(osi_trace_probe_id_t id)
{
    osi_result res;
    osi_trace_probe_t probe;

    res = osi_trace_directory_I2P(id, &probe);
    if (OSI_RESULT_OK_LIKELY(res)) {
	res = osi_trace_probe_disable(probe);
    }

    return res;
}

osi_static osi_result
enable_by_filter_helper(osi_trace_probe_t probe, void * sdata)
{
    return osi_trace_probe_enable(probe);
}

osi_static osi_result
disable_by_filter_helper(osi_trace_probe_t probe, void * sdata)
{
    return osi_trace_probe_disable(probe);
}

osi_result
osi_trace_probe_enable_by_filter(const char * filter)
{
    return osi_trace_directory_foreach(filter,
				       &enable_by_filter_helper,
				       osi_NULL);
}

osi_result
osi_trace_probe_disable_by_filter(const char * filter)
{
    return osi_trace_directory_foreach(filter,
				       &disable_by_filter_helper,
				       osi_NULL);
}

osi_result
osi_trace_probe_rgy_PkgInit(void)
{
    osi_result res = OSI_OK;
#if defined(OSI_USERSPACE_ENV)
    res = osi_trace_probe_rgy_msg_PkgInit();
#endif
    return res;
}

osi_result
osi_trace_probe_rgy_PkgShutdown(void)
{
    osi_result res = OSI_OK;
#if defined(OSI_USERSPACE_ENV)
    res = osi_trace_probe_rgy_msg_PkgInit();
#endif
    return res;
}
