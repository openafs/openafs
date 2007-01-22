/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_thread.h>

void
osi_thread_options_Set(osi_thread_options_t * opt,
		       osi_thread_options_param_t param,
		       long val)
{
    switch (param) {
    case OSI_THREAD_OPTION_DETACHED:
	opt->detached = (osi_uint8) val;
	break;
    case OSI_THREAD_OPTION_TRACE_ALLOWED:
	opt->trace_allowed = (osi_uint8) val;
	break;
    default:
	break;
    }
}
