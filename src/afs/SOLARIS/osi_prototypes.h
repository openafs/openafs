/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_PROTOTYPES_H_
#define _OSI_PROTOTYPES_H_

/* osi_file.c */
extern afs_rwlock_t afs_xosi;

/* osi_vnodeops.c */
int afs_putapage(struct vnode *vp, struct page *pages,
#if	defined(AFS_SUN56_ENV)
		 u_offset_t * offp,
#else
		 u_int * offp,
#endif
#if     defined(AFS_SUN58_ENV)
		 size_t * lenp,
#else
		 u_int * lenp,
#endif
		 int flags, afs_ucred_t *credp);



#endif /* _OSI_PROTOTYPES_H_ */
