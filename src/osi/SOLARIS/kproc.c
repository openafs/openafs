/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_proc.h>

osi_datamodel_t
osi_proc_datamodel(void)
{
    model_t model = get_udatamodel();

    switch (model) {
    case DATAMODEL_LP64:
	return OSI_DATAMODEL_LP64;
    case DATAMODEL_ILP32:
	return OSI_DATAMODEL_ILP32;
    default:
	osi_Panic("unknown datamodel");
    }
}
