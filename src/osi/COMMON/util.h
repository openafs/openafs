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

#ifndef _OSI_COMMON_UTIL_H
#define _OSI_COMMON_UTIL_H 1


/*
 * back-end implementation notes:
 *
 * default implementations of all
 * major interfaces exist in src/osi/COMMON
 * their compilation may be disabled by
 * defining the following macros in the 
 * platform header:
 *
 *  OSI_UTIL_OVERRIDE_PANIC
 *    -- disables compilation of generic osi_Panic() function
 *
 *  OSI_UTIL_OVERRIDE_ASSERT
 *    -- disable definition of generic osi_Assert() macro
 *
 *  OSI_UTIL_OVERRIDE_ASSERT_FAIL
 *    -- disables compilation of generic osi_util_AssertFail() function
 *
 *  OSI_UTIL_OVERRIDE_MSG
 *    -- disable definition of generic osi_Msg macro
 *
 *  OSI_UTIL_OVERRIDE_MSG_CONSOLE
 *    -- disable definition of generic osi_Msg_console function
 *
 *  OSI_UTIL_OVERRIDE_MSG_USER
 *    -- disable definition of generic osi_Msg_user function
 *
 *  OSI_UTIL_OVERRIDE_ABORT
 *    -- disable definition of generic abort macro
 *
 *  OSI_UTIL_OVERRIDE_EXIT
 *    -- disable definition of generic exit macro
 */


#if !defined(OSI_UTIL_OVERRIDE_PANIC)
osi_extern void osi_Panic();
#endif

#if !defined(OSI_UTIL_OVERRIDE_ASSERT_FAIL)
osi_extern void osi_util_AssertFail(const char * msg, 
				    const char * file, 
				    int line);
#endif

#if !defined(OSI_UTIL_OVERRIDE_ASSERT)
#define osi_Assert(expr) \
    osi_Macro_Begin \
        if (osi_compiler_expect_false(!(expr))) { \
            osi_util_AssertFail(osi_Macro_ToString(expr), \
                                __osi_file__, \
                                __osi_line__); \
        } \
    osi_Macro_End
#endif /* !OSI_UTIL_OVERRIDE_ASSERT */


#endif /* _OSI_COMMON_UTIL_H */
