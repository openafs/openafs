/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_H
#define _OSI_OSI_H 1

/*
 * osi abstraction
 * main header file
 *
 * for the common case we can use this to 
 * centralize include blocks
 */

#include <afsconfig.h>
#include <afs/param.h>

#if defined(KERNEL)
#if !defined(UKERNEL)
#include "afs/sysincludes.h"
#endif /* !UKERNEL */
#include "afsincludes.h"
#endif /* KERNEL */

#include <osi/osi_includes.h>
#include <osi/osi_sysincludes.h>

#endif /* _OSI_OSI_H */
