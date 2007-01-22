/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_SOLARIS_KSYSCALL_H
#define	_OSI_OSI_SOLARIS_KSYSCALL_H

#define osi_syscall_suser_check(retcode) \
    osi_Macro_Begin \
        if (!osi_proc_current_suser()) { \
            *(retcode) = EACCES; \
        } \
    osi_Macro_End

#endif /* _OSI_OSI_SOLARIS_KSYSCALL_H */
