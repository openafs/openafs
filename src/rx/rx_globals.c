/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* RX:  Globals for internal use, basically */

/* Enable data initialization when the header file is included */
#define INIT(stuff) = stuff
#if defined(AFS_NT40_ENV) && defined(AFS_PTHREAD_ENV)
#define EXT __declspec(dllexport)
#else
#define EXT
#endif

#ifdef KERNEL
#include	"../afs/param.h"
#ifndef UKERNEL
#include "../h/types.h"
#else /* !UKERNEL */
#include	"../afs/sysincludes.h"
#endif /* !UKERNEL */
#else /* KERNEL */
#include	<afs/param.h>
#endif /* KERNEL */

#include "rx_globals.h"
