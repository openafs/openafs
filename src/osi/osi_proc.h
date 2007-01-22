/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_PROC_H
#define _OSI_OSI_PROC_H 1


/*
 * platform-independent osi_proc API
 *
 * all interfaces in this file require
 * defined(OSI_IMPLEMENTS_PROC)
 *
 *  osi_proc_p -- opaque handle to a process
 *  osi_proc_t -- process info structure type
 *  osi_proc_id_t -- process id type
 *
 * process control interfaces:
 *  osi_proc_id_t osi_proc_fork();
 *    -- fork a new process
 *
 *  void osi_proc_signal(osi_proc_p, int signum);
 *    -- send a signal to a process
 *
 *  void osi_proc_exit(int);
 *    -- process exit with return code
 *
 *  osi_result osi_proc_env_get(const char * key, char ** value);
 *    -- return environment value associated with variable name $key$
 *
 *  osi_result osi_proc_env_set(const char * key, const char * value,
 *                              int allow_overwrite);
 *    -- set environment variable $key$ to value $value$
 *
 *  osi_result osi_proc_env_unset(const char * key);
 *    -- unset environment variable $key$
 *
 * process introspection interfaces (osi_proc_p ignored in userspace):
 *  osi_proc_p osi_proc_current();
 *    -- get the current process pointer
 *
 *  osi_proc_id_t osi_proc_current_id();
 *    -- get the current process id
 *
 *  osi_proc_id_t osi_proc_id(osi_proc_p);
 *    -- get the process id
 *
 *  osi_proc_id_t osi_proc_ppid(osi_proc_p);
 *    -- get the parent process id
 *
 *  osi_user_id_t osi_proc_uid(osi_proc_p);
 *    -- get the process uid
 *
 *  osi_group_id_t osi_proc_gid(osi_proc_p);
 *    -- get the process gid
 *
 *  osi_datamodel_t osi_proc_datamodel();
 *    -- get the process datamodel
 *
 *  int osi_proc_datamodel_int();
 *    -- get the user datamodel int size
 *
 *  int osi_proc_datamodel_long();
 *    -- get the user datamodel long size
 *
 *  int osi_proc_datamodel_pointer();
 *    -- get the user datamodel pointer size
 *
 * the following interfaces require
 * defined(KERNEL)
 *
 *  osi_proc_p osi_proc_parent();
 *    -- get the parent process pointer
 *
 *  int osi_proc_current_suser();
 *    -- returns nonzero if superuser, 0 otherwise
 */

/* now include the right back-end implementation header */
#if defined(OSI_KERNELSPACE_ENV)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kproc.h>
#elif defined(OSI_LINUX20_ENV)
#include <osi/LINUX/kproc.h>
#else
#include <osi/LEGACY/kproc.h>
#endif

#else /* !OSI_KERNELSPACE_ENV */

#include <osi/COMMON/uproc.h>

#endif /* !OSI_KERNELSPACE_ENV */


#include <osi/COMMON/proc.h>


#endif /* _OSI_OSI_PROC_H */
