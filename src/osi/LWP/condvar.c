/*
 * Copyright 2005-2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_condvar.h>

void
osi_condvar_Init(osi_condvar_t * cv, osi_condvar_options_t * opts)
{
    osi_condvar_options_Copy(&cv->opts, opts);
}
