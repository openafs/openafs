/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * AIX OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_


#undef osi_ThreadUnique
#define osi_ThreadUnique()	thread_self()

#define	afs_hz	    HZ
extern long time;
#define osi_Time() (time)

typedef struct ucred afs_ucred_t;
typedef struct proc afs_proc_t;

#define afs_bufferpages v.v_bufhw

#define osi_vnhold(avc, r) do { VN_HOLD(AFSTOV(avc)); } while (0)

#undef gop_lookupname
#define	gop_lookupname(fnamep,segflg,followlink,compvpp) \
	lookupvp((fnamep), (followlink), (compvpp), &afs_osi_cred)

#undef gop_lookupname_user
#define	gop_lookupname_user(fnamep,segflg,followlink,compvpp) \
	lookupvp((fnamep), (followlink), (compvpp), &afs_osi_cred)

#undef afs_suser

#undef setuerror
#undef getuerror
/* #undef getpid		getpid() provided by native kernel */
#include <ulimit.h>
#define get_ulimit()		(ulimit(GET_FSIZE, 0) << UBSHIFT)

#include <sys/lockl.h>
#include <sys/lock_def.h>
#include <sys/lock_alloc.h>
#include <sys/sleep.h>

extern simple_lock_data afs_global_lock;
#define AFS_GLOCK()	do {						\
			    if (lock_mine((void *)&afs_global_lock))	\
				osi_Panic("AFS_GLOCK %s %d",		\
					  __FILE__, __LINE__);		\
			    simple_lock((void *)&afs_global_lock);	\
			} while(0)
#define AFS_GUNLOCK()	do {						\
			    if (!lock_mine((void *)&afs_global_lock))	\
				osi_Panic("AFS_GUNLOCK %s %d",		\
					  __FILE__, __LINE__);		\
			    simple_unlock((void *)&afs_global_lock);	\
			} while(0)
#define ISAFS_GLOCK()	lock_mine((void *)&afs_global_lock)

#if defined(AFS_AIX41_ENV)
#define osi_InitGlock() \
	do {								\
	    lock_alloc((void *)&afs_global_lock, LOCK_ALLOC_PIN, 1, 1);	\
	    simple_lock_init((void *)&afs_global_lock);			\
	} while(0)
#else
#define osi_InitGlock() \
	mutex_init(&afs_global_lock, "afs_global_lock", MUTEX_DEFAULT, NULL)
#endif

/* Reading the current proc name from kernelspace is difficult. It is
 * probably possible via indexing into v.vb_proc, but for now don't bother.
 * To actually obtain the proc name, look at afs_procsize_init and
 * src/afs/AIX/osi_gcpags.c for how to look at the process list */
#define osi_procname(procname, size) strncpy(procname, "", size)

#endif /* _OSI_MACHDEP_H_ */
