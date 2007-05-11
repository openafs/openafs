/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_demux.h>

osi_result
osi_demux_options_Set(osi_demux_options_t * opt,
		      osi_demux_options_param_t param,
		      osi_options_val_t * val)
{
    osi_result res = OSI_FAIL;

    switch (param) {
    case OSI_DEMUX_OPTION_TRACE_ALLOWED:
	if (val->type == OSI_OPTION_VAL_TYPE_BOOL) {
	    opt->trace_allowed = val->val.v_bool;
	    res = OSI_OK;
	}
	break;
    case OSI_DEMUX_OPTION_TRACE_ENABLED:
	if (val->type == OSI_OPTION_VAL_TYPE_BOOL) {
	    opt->trace_enabled = val->val.v_bool;
	    res = OSI_OK;
	}
	break;
    }

    return res;
}

osi_result
osi_demux_action_options_Set(osi_demux_action_options_t * opt,
			     osi_demux_action_options_param_t param,
			     osi_options_val_t * val)
{
    osi_result res = OSI_FAIL;

    switch (param) {
    case OSI_DEMUX_ACTION_OPTION_ERROR_IS_FATAL:
	if (val->type == OSI_OPTION_VAL_TYPE_BOOL) {
	    opt->error_is_fatal = val->val.v_bool;
	    res = OSI_OK;
	}
	break;
    case OSI_DEMUX_ACTION_OPTION_TERMINAL:
	if (val->type == OSI_OPTION_VAL_TYPE_BOOL) {
	    opt->terminal = val->val.v_bool;
	    res = OSI_OK;
	}
	break;
    }

    return res;
}
