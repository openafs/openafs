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
 * trace data generator library
 * probe point data registry
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_string.h>
#include <osi/osi_object_cache.h>
#include <trace/generator/activation.h>
#include <trace/generator/probe_impl.h>
#include <trace/generator/probe_mail.h>
#include <trace/generator/probe.h>
#include <trace/generator/directory.h>
#include <trace/generator/directory_impl.h>


osi_mem_object_cache_t * osi_trace_probe_rgy_cache = osi_NULL;

osi_static int
osi_trace_probe_rgy_cache_ctor(void * buf, void * sdata, int flags);
osi_static void
osi_trace_probe_rgy_cache_dtor(void * buf, void * sdata);


osi_result
osi_trace_probe_id(osi_trace_probe_t probe, osi_trace_probe_id_t * probe_id)
{
    *probe_id = probe->probe_id;
    return OSI_OK;
}


osi_static int
osi_trace_probe_rgy_cache_ctor(void * buf, void * sdata, int flags)
{
    struct osi_trace_probe * probe = buf;

    osi_mutex_Init(&probe->probe_lock,
		   osi_trace_impl_mutex_opts());

    return 0;
}

osi_static void
osi_trace_probe_rgy_cache_dtor(void * buf, void * sdata)
{
    struct osi_trace_probe * probe = buf;

    osi_mutex_Destroy(&probe->probe_lock);
}


#if defined(OSI_ENV_USERSPACE)
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/mail/handler.h>
#include <trace/USERSPACE/mail.h>

/* XXX this code is not ready for use yet */
osi_static osi_result
osi_trace_probe_register_notify(osi_trace_probe_t probe,
				const char * probe_name)
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
    osi_string_ncpy(payload->probe_name, probe_name, sizeof(payload->probe_name));

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
#endif /* OSI_ENV_USERSPACE */

/*
 * register a new probe with the tracing framework
 *
 * [OUT] probe_out  -- address in which to store opaque handle to probe object
 * [IN] probe_name  -- probe name string
 * [OUT] probe_id_out  -- address in which to store allocated probe id
 * [IN] mode        -- new probe default mode
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   see osi_trace_directory_probe_id_alloc()
 */
osi_result
osi_trace_probe_register(osi_trace_probe_t * probe_out,
			 const char * probe_name,
			 osi_trace_probe_id_t * probe_id_out,
			 int mode)
{
    osi_result res = OSI_OK;
    struct osi_trace_probe * probe;
    osi_mutex_options_t opt;

    probe = (struct osi_trace_probe *)
	osi_mem_object_cache_alloc(osi_trace_probe_rgy_cache);
    if (osi_compiler_expect_false(probe == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto done;
    }

    probe->activation_count = 0;
    res = osi_trace_directory_probe_id_alloc(&probe->probe_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	osi_mem_object_cache_free(osi_trace_probe_rgy_cache, probe);
	goto done;
    }

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

/*
 * enable a probe given its opaque object handle
 *
 * [IN] probe  -- opaque probe object handle
 *
 * returns:
 *   OSI_OK, or
 *   see osi_TracePoint_Activate()
 */
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

/*
 * disable a probe given its opaque object handle
 *
 * [IN] probe  -- opaque probe object handle
 *
 * returns:
 *   OSI_OK, or
 *   see osi_TracePoint_Deactivate()
 */
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

/*
 * enable a probe given its id
 *
 * [IN] id  -- probe id
 *
 * returns:
 *   see osi_trace_probe_I2P()
 *   see osi_trace_probe_enable()
 */
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

/*
 * disable a probe given its id
 *
 * [IN] id  -- probe id
 *
 * returns:
 *   see osi_trace_probe_I2P()
 *   see osi_trace_probe_disable()
 */
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
enable_by_filter_helper(osi_trace_probe_t probe, void * rock)
{
    osi_uint32 * nhits = rock;
    (*nhits)++;
    return osi_trace_probe_enable(probe);
}

osi_static osi_result
disable_by_filter_helper(osi_trace_probe_t probe, void * rock)
{
    osi_uint32 * nhits = rock;
    (*nhits)++;
    return osi_trace_probe_disable(probe);
}

/*
 * enable all probes matching a filter expression
 *
 * [IN] filter  -- probe name filter
 * [OUT] nhits  -- address in which to store filter hit count
 *
 * preconditions:
 *   nhits MUST NOT be NULL
 *
 * returns:
 *   see osi_trace_directory_foreach()
 */
osi_result
osi_trace_probe_enable_by_filter(const char * filter,
				 osi_uint32 * nhits)
{
    *nhits = 0;
    return osi_trace_directory_foreach(filter,
				       &enable_by_filter_helper,
				       nhits);
}

/*
 * disable all probes matching a filter expression
 *
 * [IN] filter  -- probe name filter
 * [OUT] nhits  -- address in which to store filter hit count
 *
 * preconditions:
 *   nhits MUST NOT be NULL
 *
 * returns:
 *   see osi_trace_directory_foreach()
 */
osi_result
osi_trace_probe_disable_by_filter(const char * filter,
				  osi_uint32 * nhits)
{
    *nhits = 0;
    return osi_trace_directory_foreach(filter,
				       &disable_by_filter_helper,
				       nhits);
}

osi_result
osi_trace_probe_rgy_PkgInit(void)
{
    osi_result res = OSI_OK;
    osi_size_t align;

    res = osi_cache_max_alignment(&align);
    if (OSI_RESULT_FAIL(res)) {
	align = 64;
    }

    osi_trace_probe_rgy_cache = 
	osi_mem_object_cache_create("osi_trace_probe_rgy_cache",
				    sizeof(struct osi_trace_probe),
				    align,
				    osi_NULL,
				    &osi_trace_probe_rgy_cache_ctor,
				    &osi_trace_probe_rgy_cache_dtor,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_trace_probe_rgy_cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

#if defined(OSI_ENV_USERSPACE)
    res = osi_trace_probe_rgy_msg_PkgInit();
#endif

 error:
    return res;
}

osi_result
osi_trace_probe_rgy_PkgShutdown(void)
{
    osi_result res = OSI_OK;

#if defined(OSI_ENV_USERSPACE)
    res = osi_trace_probe_rgy_msg_PkgShutdown();
#endif

    osi_mem_object_cache_destroy(osi_trace_probe_rgy_cache);

    return res;
}
