/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifdef	KERNEL
#include "afs/param.h"
#ifndef	UKERNEL
#include "../h/types.h"
#else /* !UKERNEL */
#include "afs/sysincludes.h"
#endif /* !UKERNEL */
#include "../rx/rx.h"
#else /* KERNEL */
#include <afs/param.h>
#include "rx.h"
#endif /* KERNEL */

/* The null security object.  No authentication, no nothing. */

static struct rx_securityOps null_ops;
static struct rx_securityClass null_object = {&null_ops, 0, 0};

struct rx_securityClass *rxnull_NewServerSecurityObject()
{
    return &null_object;
}

struct rx_securityClass *rxnull_NewClientSecurityObject()
{
    return &null_object;
}
