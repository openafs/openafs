/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


#include <osi/osi_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_mem_local.h>

/*
 * thread-local memory support
 */

pthread_key_t osi_mem_local_pthread_key;

osi_result
osi_mem_local_os_PkgInit(void)
{
    osi_result res = OSI_OK;

    if (pthread_key_create(&osi_mem_local_pthread_key, osi_NULL) != 0) {
	res = OSI_FAIL;
    }

    return res;
}

