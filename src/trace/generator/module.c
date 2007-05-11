/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_mutex.h>
#include <osi/osi_list.h>
#include <osi/osi_string.h>
#include <trace/gen_rgy.h>
#include <trace/generator/module.h>
#include <trace/generator/module_mail.h>
#if defined(OSI_ENV_KERNELSPACE)
#include <trace/KERNEL/gen_rgy.h>
#endif

/*
 * modular tracepoint table support
 */

/*
 * registry of all trace modules
 *
 * module_version_cksum is used for consumer i2n cache invalidation
 *  XXX at the moment this is a simple sum.  this needs to become
 *      a checksum (md5 should be adequate, as collision attack
 *      resistance is not a concern)
 */
struct osi_trace_modules {
    osi_list_head_volatile module_list;
    osi_trace_module_id_t osi_volatile id_counter;
    osi_uint32 osi_volatile module_version_cksum;
    osi_trace_module_cksum_type_t module_version_cksum_type;
    osi_uint32 osi_volatile probe_count;
    osi_mutex_t lock;
};

struct osi_trace_modules osi_trace_modules;

osi_static osi_result osi_trace_module_fill_info(osi_trace_module_header_t * hdr,
						 osi_trace_module_info_t * info);


osi_static osi_result
osi_trace_module_fill_info(osi_trace_module_header_t * hdr,
			   osi_trace_module_info_t * info)
{
    info->id = hdr->id;
    info->version = hdr->version;
    info->probe_count = hdr->probe_count;
    osi_string_lcpy(info->name, hdr->name, sizeof(info->name));
    osi_string_lcpy(info->prefix, hdr->prefix, sizeof(info->prefix));
    return OSI_OK;
}

osi_result
osi_trace_module_register(osi_trace_module_header_t * hdr)
{
    osi_result res = OSI_OK;

    osi_mutex_Lock(&osi_trace_modules.lock);
    osi_list_Append(&osi_trace_modules.module_list,
		    hdr,
		    osi_trace_module_header_t,
		    module_list);
    osi_trace_modules.module_version_cksum += hdr->version;
    osi_trace_modules.probe_count += hdr->probe_count;
    hdr->id = osi_trace_modules.id_counter++;
    osi_mutex_Unlock(&osi_trace_modules.lock);

    return res;
}

osi_result
osi_trace_module_unregister(osi_trace_module_header_t * hdr)
{
    osi_result res = OSI_FAIL;
    osi_trace_module_header_t *mh, *nmh;

    osi_mutex_Lock(&osi_trace_modules.lock);
    for (osi_list_Scan(&osi_trace_modules.module_list,
		       mh, nmh,
		       osi_trace_module_header_t,
		       module_list)) {
	if (mh == hdr) {
	    osi_list_Remove(mh,
			    osi_trace_module_header_t,
			    module_list);
	    osi_trace_modules.probe_count -= hdr->probe_count;
	    res = OSI_OK;
	    break;
	}
    }
    osi_mutex_Unlock(&osi_trace_modules.lock);

    return res;
}

/*
 * get global metadata about the module package of the generator
 *
 * [OUT] info  -- generator info structure
 *
 * returns:
 *   OSI_OK on success
 *   see osi_trace_gen_id()
 *   see osi_trace_directory_probe_id_max()
 */
osi_result
osi_trace_module_info(osi_trace_generator_info_t * info)
{
    osi_result res, code = OSI_OK;
    osi_trace_gen_id_t gen_id;
    osi_trace_probe_id_t max_probe_id;

    osi_mutex_Lock(&osi_trace_modules.lock);
    info->probe_count = osi_trace_modules.probe_count;
    info->module_count = osi_trace_modules.id_counter;
    info->module_version_cksum = osi_trace_modules.module_version_cksum;
    info->module_version_cksum_type = osi_trace_modules.module_version_cksum_type;
    osi_mutex_Unlock(&osi_trace_modules.lock);

    info->programType = osi_config_programType();
    info->epoch = osi_config_epoch();

    res = osi_trace_gen_id(&gen_id);
    if (OSI_RESULT_OK(res)) {
	info->gen_id = gen_id;
    } else {
	info->gen_id = 0;
	code = res;
    }

    res = osi_trace_directory_probe_id_max(&max_probe_id);
    if (OSI_RESULT_OK(res)) {
	info->probe_id_max = max_probe_id;
    } else {
	info->probe_id_max = 0;
	code = res;
    }

    return code;
}

osi_result
osi_trace_module_lookup(osi_trace_module_id_t id,
			osi_trace_module_info_t * info)
{
    osi_result res = OSI_FAIL;
    osi_trace_module_header_t * mh;

    osi_mutex_Lock(&osi_trace_modules.lock);
    if (id < (osi_trace_modules.id_counter >> 1)) {
	for (osi_list_Scan_Immutable(&osi_trace_modules.module_list,
				     mh,
				     osi_trace_module_header_t,
				     module_list)) {
	    if (mh->id == id) {
		res = osi_trace_module_fill_info(mh, info);
		break;
	    }
	}
    } else {
	for (osi_list_ScanBackwards_Immutable(&osi_trace_modules.module_list,
					      mh,
					      osi_trace_module_header_t,
					      module_list)) {
	    if (mh->id == id) {
		res = osi_trace_module_fill_info(mh, info);
		break;
	    }
	}
    }
    osi_mutex_Unlock(&osi_trace_modules.lock);

    return res;
}

osi_result
osi_trace_module_lookup_by_name(const char * name,
				osi_trace_module_info_t * info)
{
    osi_result res = OSI_FAIL;
    osi_trace_module_header_t * mh;

    osi_mutex_Lock(&osi_trace_modules.lock);
    for (osi_list_Scan_Immutable(&osi_trace_modules.module_list,
				 mh,
				 osi_trace_module_header_t,
				 module_list)) {
	if (!osi_string_cmp(name, mh->name)) {
	    res = osi_trace_module_fill_info(mh, info);
	    break;
	}
    }
    osi_mutex_Unlock(&osi_trace_modules.lock);

    return res;
}

osi_result
osi_trace_module_lookup_by_prefix(const char * prefix,
				  osi_trace_module_info_t * info)
{
    osi_result res = OSI_FAIL;
    osi_trace_module_header_t * mh;

    osi_mutex_Lock(&osi_trace_modules.lock);
    for (osi_list_Scan_Immutable(&osi_trace_modules.module_list,
				 mh,
				 osi_trace_module_header_t,
				 module_list)) {
	if (!osi_string_cmp(prefix, mh->prefix)) {
	    res = osi_trace_module_fill_info(mh, info);
	    break;
	}
    }
    osi_mutex_Unlock(&osi_trace_modules.lock);

    return res;
}

osi_result
osi_trace_module_PkgInit(void)
{
    osi_result res;

    osi_mutex_Init(&osi_trace_modules.lock,
		   osi_trace_impl_mutex_opts());

    osi_list_Init(&osi_trace_modules.module_list);
    osi_trace_modules.id_counter = 0;
    osi_trace_modules.probe_count = 0;
    osi_trace_modules.module_version_cksum = 0;
    osi_trace_modules.module_version_cksum_type = OSI_TRACE_MODULE_CKSUM_TYPE_SUM;

#if defined(OSI_ENV_USERSPACE)
    res =  osi_trace_module_msg_PkgInit();
#else
    res = OSI_OK;
#endif

    return res;
}

osi_result
osi_trace_module_PkgShutdown(void)
{
    osi_result res;
    osi_trace_module_header_t * mh, * nmh;

#if defined(OSI_ENV_USERSPACE)
    res = osi_trace_module_msg_PkgShutdown();
#else
    res = OSI_OK;
#endif

    osi_mutex_Lock(&osi_trace_modules.lock);
    for (osi_list_Scan(&osi_trace_modules.module_list,
		       mh, nmh,
		       osi_trace_module_header_t,
		       module_list)) {
	osi_list_Remove(mh,
			osi_trace_module_header_t,
			module_list);
    }
    osi_trace_modules.id_counter = 0;
    osi_mutex_Unlock(&osi_trace_modules.lock);

    osi_mutex_Destroy(&osi_trace_modules.lock);

    return res;
}
