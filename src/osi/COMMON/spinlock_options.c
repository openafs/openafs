/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_spinlock.h>
#include <osi/COMMON/spinlock_options.h>

void
osi_spinlock_options_Set(osi_spinlock_options_t * opt,
			 osi_spinlock_options_param_t param,
			 long val)
{
    switch (param) {
    case OSI_SPINLOCK_OPTION_PREEMPTIVE_ONLY:
	opt->preemptive_only = (osi_uint8) val;
	break;
    case OSI_SPINLOCK_OPTION_TRACE_ALLOWED:
	opt->trace_allowed = (osi_uint8) val;
	break;
    case OSI_SPINLOCK_OPTION_TRACE_ENABLED:
	opt->trace_enabled = (osi_uint8) val;
	break;
    default:
	break;
    }
}
