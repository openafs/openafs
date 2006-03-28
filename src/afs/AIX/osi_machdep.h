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

#define	AFS_UCRED	ucred
#define	AFS_PROC	struct proc

#define afs_bufferpages v.v_bufhw

#define osi_vnhold(avc, r) do { (avc)->vrefCount++; } while (0)
#undef AFS_FAST_HOLD
#define AFS_FAST_HOLD(vp) (vp)->vrefCount++
#undef AFS_FAST_RELE
#define AFS_FAST_RELE(vp) (vp)->vrefCount--

#undef gop_lookupname
#define	gop_lookupname(fnamep,segflg,followlink,compvpp) \
	lookupvp((fnamep), (followlink), (compvpp), &afs_osi_cred)

#undef afs_suser

#undef setuerror
#undef getuerror
/* #undef getpid		getpid() provided by native kernel */
#include <ulimit.h>
#define get_ulimit()		(ulimit(GET_FSIZE, 0) << UBSHIFT)

#ifdef KERNEL
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

#define ifnet_flags(x) (x?(x)->if_flags:0)
#endif

#endif /* _OSI_MACHDEP_H_ */
