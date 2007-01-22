/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_UPROC_H
#define _OSI_COMMON_UPROC_H 1

/*
 * userspace process api
 */

typedef pid_t osi_proc_id_t;
typedef pid_t osi_proc_p;


#define osi_proc_fork() fork()
#define osi_proc_signal(pp, sig) osi_signal_raise((pp), (sig))
#define osi_proc_exit(x) exit(x)

#define osi_proc_current() getpid()
#define osi_proc_id(x) x
#define osi_proc_ppid(x) getppid()
#define osi_proc_uid(x) getuid()
#define osi_proc_gid(x) getgid()

#define osi_proc_datamodel() osi_datamodel()


#if (OSI_ENV_INLINE_INCLUDE)
#include <osi/COMMON/uproc_inline.h>
#endif

#endif /* _OSI_COMMON_UPROC_H */
