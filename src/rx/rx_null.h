/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef RX_NULL_HEADER
#define RX_NULL_HEADER

#ifdef	KERNEL
#include "../rx/rx.h"
#else /* KERNEL */
#include "rx.h"
#endif /* KERNEL */

/* The null security object.  No authentication, no nothing. */

extern struct rx_securityClass *rxnull_NewServerSecurityObject();
extern struct rx_securityClass *rxnull_NewClientSecurityObject();

#endif /* RX_NULL_HEADER */
