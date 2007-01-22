/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi condvar interface
 * linux kernel condvar emulation
 * inline functions
 */

#define OSI_BUILD_INLINES 1
#include <osi/osi_impl.h>
#include <osi/osi_condvar.h>
#include <osi/LINUX/kcondvar.h>
#include <osi/LINUX/kcondvar_inline.h>
