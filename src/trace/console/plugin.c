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
 * trace console
 * plugin initialization routines
 */

#include <osi/osi_impl.h>
#include <osi/osi_list.h>
#include <trace/console/plugin.h>

osi_static osi_list_head plugin_list = { &plugin_list, &plugin_list };
osi_static int plugin_list_inited = 0;

/*
 * register a plugin
 * this should be called from your shared object _init() function
 *
 * [IN] node  -- previously declared and populated registration node pointer
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_console_plugin_register(osi_trace_console_plugin_registration_t * node)
{
    if (!plugin_list_inited) {
	osi_list_Init(&plugin_list);
    }

    osi_list_Append(&plugin_list,
		    node,
		    osi_trace_console_plugin_registration_t,
		    plugin_list);
    return OSI_OK;
}


osi_result
osi_trace_console_plugin_PkgInit(void)
{
    osi_result res, code = OSI_OK;
    osi_trace_console_plugin_registration_t * node;

    for (osi_list_Scan_Immutable(&plugin_list,
				 node,
				 osi_trace_console_plugin_registration_t,
				 plugin_list)) {
	if (node->initializer) {
	    res = (*node->initializer)(node->sdata);
	    if (OSI_RESULT_FAIL(res)) {
		code = res;
		if (node->error_is_fatal) {
		    break;
		}
	    }
	}
    }

    return code;
}

osi_result
osi_trace_console_plugin_PkgShutdown(void)
{
    osi_result res, code = OSI_OK;
    osi_trace_console_plugin_registration_t * node;

    for (osi_list_Scan_Immutable(&plugin_list,
				 node,
				 osi_trace_console_plugin_registration_t,
				 plugin_list)) {
	if (node->finalizer) {
	    res = (*node->finalizer)(node->sdata);
	    if (OSI_RESULT_FAIL(res)) {
		code = res;
		if (node->error_is_fatal) {
		    break;
		}
	    }
	}
    }

    return code;
}
