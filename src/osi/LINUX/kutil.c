/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006 Sine Nomine Associates
 */

/* this code was pulled from rx_kcommon.c */

#include <osi/osi_impl.h>

/*
 * linux-specific kernel panic support
 */


#if defined(OSI_LINUX26_ENV)
void
osi_util_AssertFail(const char *expr, const char *file, int line)
{
    printk(KERN_CRIT "assertion failed: %s, file: %s, line: %d\n", 
	   expr, file, line);
}
#endif /* OSI_LINUX26_ENV */

