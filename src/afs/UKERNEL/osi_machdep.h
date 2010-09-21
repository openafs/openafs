/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * UKERNEL OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

#include "afs/sysincludes.h"

#define MAX_OSI_PATH            1024
#define MAX_OSI_FILES           1024
#define MAX_OSI_LINKS           25
#define OSI_WAITHASH_SIZE       128	/* must be power of two */
#define MAX_HOSTADDR            32

/* XXX
 *
 * UKERNEL cannot trivially make use of typedef for afs_ucred_t and
 * afs_proc_t, as in the present design, the types are already introduced
 * with incorrect reference, so fall back to preprocessor for now
 */

#define afs_ucred_t struct usr_ucred
#define afs_proc_t struct usr_proc

#define AFS_KALLOC(A)           afs_osi_Alloc(A)

/*
 * Time related macros
 */
#define	afs_hz	    HZ
#define osi_Time() (time(NULL))

#undef gop_lookupname
#define gop_lookupname(fnamep,segflg,followlink,compvpp) lookupname((fnamep),(segflg),(followlink),(compvpp))
#undef gop_lookupname_user
#define gop_lookupname_user(fnamep,segflg,followlink,compvpp) lookupname((fnamep),(segflg),(followlink),(compvpp))

#define osi_vnhold(avc, r)  do { VN_HOLD(AFSTOV(avc)); } while(0)

#if 0
#define	afs_suser(x)	    suser(x)
#endif
/*
 * Global lock support.
 */

extern usr_thread_t afs_global_owner;
extern usr_mutex_t afs_global_lock;

#define ISAFS_GLOCK() (usr_thread_self() == afs_global_owner)

#define AFS_GLOCK() \
    do { \
	usr_mutex_lock(&afs_global_lock); \
	afs_global_owner = usr_thread_self(); \
    } while(0)
#define AFS_GUNLOCK() \
    do { \
	AFS_ASSERT_GLOCK(); \
	memset(&afs_global_owner, 0, sizeof(usr_thread_t)); \
 	usr_mutex_unlock(&afs_global_lock); \
    } while(0)
#define AFS_ASSERT_GLOCK() \
    do { if (!ISAFS_GLOCK()) { osi_Panic("afs global lock not held"); } } while(0)
#define osi_GlockInit() \
    usr_mutex_init(&afs_global_lock, "afs_global_lock", MUTEX_DEFAULT, NULL)

extern int afs_bufferpages;

extern int afs_suser(void *credp);

#define setuerror(erval) get_user_struct()->u_error = (erval)
#define getuerror(erval) get_user_struct()->u_error
#define osi_curcred()    get_user_struct()->u_cred

#define osi_procname(procname, size) strncpy(procname, "(unknown)", size)
#endif /* _OSI_MACHDEP_H_ */
