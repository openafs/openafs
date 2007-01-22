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

#ifndef _OSI_LINUX_KUTIL_H
#define _OSI_LINUX_KUTIL_H


/* 
 * for linux 2.6 case, we override default behavior
 * so that we can use the inline BUG() macro
 */
#if defined(OSI_LINUX26_ENV)

#define OSI_UTIL_OVERRIDE_PANIC 1
#define OSI_UTIL_OVERRIDE_ASSERT 1
#define OSI_UTIL_OVERRIDE_ASSERT_FAIL 1

osi_extern void osi_util_AssertFail(const char * msg, 
				    const char * file, 
				    int line);

#define osi_Panic(msg...) \
    osi_Macro_Begin \
        printk(KERN_CRIT "openafs_osi: " msg); \
        BUG(); \
    osi_Macro_End

#define osi_Assert(expr) \
    osi_Macro_Begin \
        if (!(expr)) { \
            osi_util_AssertFail(osi_Macro_ToString(expr), \
				__osi_file__, \
				__osi_line__); \
            BUG(); \
        } \
    osi_Macro_End

#endif /* OSI_LINUX26_ENV */


#endif /* _OSI_LINUX_KUTIL_H */
